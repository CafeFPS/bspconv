#include "stdafx.h"
#include "stltools.h"
#include "binstream.h"
#include "studio.h"
#include "entity_partition.h"
#include "bspfile.h"

#define MAX_COLLISION_CHUNK_SIZE 0x78

const char* CEntityPartitionMgr::ParseQuoted(const char* const subKeyStart, const char* const subKeyEnd, std::string& quoted)
{
	assert(subKeyStart && subKeyEnd);
	assert(quoted.empty());

	const char* it = subKeyStart;
	it = strchr(it, '"'); // Leading '"'.

	if (it && it < subKeyEnd)
	{
		it++;
		if (!*it)
		{
			// Unexpected end???
			return nullptr;
		}

		while (*it != '"')
		{
			if (it >= subKeyEnd)
			{
				// Exceeds object end; missing trailing '"'???
				assert(0);
				return nullptr;
			}

			quoted += *it;
			it++;
		}

		return it;
	}

	// NULL or exceeds end.
	return nullptr;
}

const char* CEntityPartitionMgr::ParseKeyValue(const char* const subKeyStart, const char* const subKeyEnd, Object_t& subKey)
{
	Field_t kv;
	const char* keyEnd = ParseQuoted(subKeyStart, subKeyEnd, kv.key);

	if (!keyEnd) // Should point to the key's ending '"'.
	{
		return nullptr;
	}

	const char* valueEnd = ParseQuoted(++keyEnd, subKeyEnd, kv.value);

	if (!valueEnd) // Should point to the value's ending '"'.
	{
		return nullptr;
	}

	subKey.Add(std::move(kv));
	return ++valueEnd;
}

void CEntityPartitionMgr::ParseKeyValues(const char* const subKeyStart, const char* const subKeyEnd, Object_t& subKey)
{
	const char* it = subKeyStart;

	while (it)
	{
		it = ParseKeyValue(it, subKeyEnd, subKey);
	}
}

bool CEntityPartitionMgr::ParseHeader(const char* const partitionBuffer)
{
	m_numHeaderFields = sscanf_s(partitionBuffer, "ENTITIES%d num_models=%d\n", &m_entities, &m_numModels);

	if (m_numHeaderFields == 1)
	{
		if (m_entities != 1)
		{
			return false;
		}
	}
	else if (m_numHeaderFields == 2)
	{
		if (m_entities < 2 || m_numModels < 0)
		{
			return false;
		}
	}

	return true;
}

bool CEntityPartitionMgr::ParseFromBuffer(const char* const partitionBuffer, const bool parseHeader)
{
	if (partitionBuffer && partitionBuffer[0])
	{
		if (parseHeader && !ParseHeader(partitionBuffer))
		{
			assert(0);
			return false;
		}

		const char* objectIt = strchr(partitionBuffer, '{');
		if (!objectIt || !*objectIt)
		{
			// Partition file is empty (not an error).
			return false;
		}

		while (objectIt)
		{
			if (*objectIt == '{')
			{
				const char* const subKeyEnd = strchr(objectIt, '}');

				if (!subKeyEnd)
				{
					// Truncated entity partition!!!
					assert(0);
					return false;
				}

				Object_t subKey;
				ParseKeyValues(objectIt, subKeyEnd, subKey);

				m_base.push_back(subKey);
				objectIt = strchr(subKeyEnd, '{');
			}
		}
	}
	else
	{
		// User passed in empty or invalid buffer.
		assert(0);
		return false;
	}

	return true;
}

bool CEntityPartitionMgr::Write(const char* const fileName)
{
	CIOStream inEntityPartition;
	if (!inEntityPartition.Open(fileName, CIOStream::WRITE | CIOStream::BINARY))
	{
		return false;
	}

	std::string outBuf;
	WriteToString(outBuf);

	inEntityPartition.WriteString(outBuf);

	// Entity partition files must always end with a '\0'!!!
	inEntityPartition.Write('\0');
	return true;
}

void CEntityPartitionMgr::WriteToString(std::string& buffer)
{
	if (m_numHeaderFields > 0 && m_numHeaderFields <= 2)
	{
		std::string header;

		if (m_numHeaderFields == 1)
		{
			header = Format("ENTITIES%02d\n", m_entities);
		}
		else if (m_numHeaderFields == 2)
		{
			header = Format("ENTITIES%02d num_models=%d\n", m_entities, m_numModels);
		}

		buffer.append(header);
	}

	for (const Object_t& sub : m_base)
	{
		buffer.append("{\n");

		for (const Field_t& kv : sub.keyValues)
		{
			buffer.append(Format("\"%s\" \"%s\"\n", kv.GetKey(), kv.GetValue()));
		}

		buffer.append("}\n");
	}
}

bool CEntityPartitionMgr::ConvertEntityPartition()
{
	// Convert all brush models from v12.1 -> v8.
	for (Object_t& sub : m_base)
	{
		std::vector<uint8_t> outVec;
		if (!DecodeBrushModel(sub, outVec))
		{
			// This object doesn't contain brush model fields
			// continue to the next one...
			continue;
		}

		if (!ConvertBrushModel(outVec))
		{
			// BUG!!!
			assert(0);
			return false;
		}

		if (!EncodeBrushModel(sub, outVec))
		{
			// BUG!!!
			assert(0);
			return false;
		}
	}

	return true;
}

bool CEntityPartitionMgr::GetBrushModelFields(Object_t& object, std::vector<Field_t*>& brushModelFields)
{
	for (size_t i = 0; i < object.keyValues.size(); i++)
	{
		Field_t* const field = &object.keyValues[i];
		const char* const attrib = field->GetKey();

		if (attrib[0] != '*')
			continue;

		if (!strstr(attrib, "*coll"))
			continue;

		brushModelFields.push_back(field);
	}

	if (brushModelFields.empty())
		return false;

	std::sort(brushModelFields.begin(), brushModelFields.end());
	return true;
}

bool CEntityPartitionMgr::DecodeBrushModel(Object_t& object, std::vector<uint8_t>& brushModelData)
{
	std::vector<Field_t*> brushModelFields;

	if (!GetBrushModelFields(object, brushModelFields))
		return false;

	for (const Field_t* const field : brushModelFields)
	{
		const std::vector<uint8_t> partColData = Base64Decode(field->GetValue());
		brushModelData.insert(brushModelData.end(), partColData.begin(), partColData.end());
	}

	return true;
}

bool CEntityPartitionMgr::EncodeBrushModel(Object_t& object, const std::vector<unsigned char>& brushModelData)
{
	std::vector<Field_t*> collisionFields;

	if (!GetBrushModelFields(object, collisionFields))
		return false;

	const size_t newCollDataSize = brushModelData.size();
	const size_t numChunks = newCollDataSize / MAX_COLLISION_CHUNK_SIZE;
	size_t remainingSize = newCollDataSize;

	std::vector<std::string> encodedCollData;

	for (size_t i = 0; i <= numChunks && remainingSize; i++)
	{
		const size_t chunkSize = (remainingSize > MAX_COLLISION_CHUNK_SIZE) ? MAX_COLLISION_CHUNK_SIZE : remainingSize;
		remainingSize -= chunkSize;

		std::string encoded = Base64Encode(reinterpret_cast<const char*>(&brushModelData[i * MAX_COLLISION_CHUNK_SIZE]), chunkSize);
		encodedCollData.emplace_back(std::move(encoded));
	}

	assert(remainingSize == NULL);

	for (size_t i = 0; i < collisionFields.size(); i++)
	{
		Field_t* const field = collisionFields[i];
		if (i < encodedCollData.size())
			field->SetValue(encodedCollData[i].c_str());
		else // New encoded collision is smaller, additional chunks can be dropped.
			object.Remove(field->GetKey());
	}

	return true;
}

bool CEntityPartitionMgr::ConvertBrushModel(std::vector<uint8_t>& brushModelData)
{
	if (brushModelData.empty())
	{
		// Called on an empty buffer!!!
		assert(0);
		return false;
	}

	// The offsets to the new fields of the struct, this new data needs to be removed:
	const size_t offset = offsetof(r5::v121::dbrushmodel_t, header.unkIndex);
	const size_t numBytesToRemove = sizeof(r5::v121::mstudiocollheader_t) - sizeof(r5::v8::mstudiocollheader_t);

	// Add padding up to # 'bytes to remove' to maintain SIMD alignment of BVH nodes.
	r5::v121::dbrushmodel_t* const pOldBrushModel = reinterpret_cast<r5::v121::dbrushmodel_t*>(&brushModelData[0]);
	brushModelData.insert(brushModelData.begin() + pOldBrushModel->header.bvhNodeIndex, numBytesToRemove, '\0');

	// Remove the new fields for v8 compatibility.
	brushModelData.erase(brushModelData.begin() + offset, brushModelData.begin() + (offset + numBytesToRemove));

	// adjust header offsets to account for the difference in structs between v10 and v12.1
	r5::v8::dbrushmodel_t* const pNewBrushModel = reinterpret_cast<r5::v8::dbrushmodel_t*>(&brushModelData[0]);

	pNewBrushModel->model.contentMasksIndex -= numBytesToRemove;
	pNewBrushModel->model.surfacePropsIndex -= numBytesToRemove;
	pNewBrushModel->model.surfaceNamesIndex -= numBytesToRemove;
	pNewBrushModel->header.vertIndex -= numBytesToRemove;
	pNewBrushModel->header.bvhLeafIndex -= numBytesToRemove;

	return true;
}
