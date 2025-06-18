#pragma once

class CEntityPartitionMgr
{
private:
	struct Field_t
	{
		inline const char* GetKey() const { return key.c_str(); }
		inline void SetKey(const char* newKey) { key = newKey; }
		inline const char* GetValue() const { return value.c_str(); }
		inline void SetValue(const char* newValue) { value = newValue; }

		std::string key;
		std::string value;
	};

	struct Object_t
	{
		inline std::vector<Field_t>::iterator Find(const char* const key)
		{
			return std::find_if(keyValues.begin(), keyValues.end(),
				[&key](const Field_t& element) { return element.key == key; });
		}
		inline void Add(Field_t& field)
		{
			keyValues.push_back(field);
		}
		inline void Add(Field_t&& field)
		{
			keyValues.push_back(field);
		}
		inline void Remove(const char* key)
		{
			const auto it = Find(key);

			if (it != keyValues.end())
			{
				keyValues.erase(it);
			}
		}

		Field_t* FindKey(const char* const key)
		{
			const auto it = Find(key);

			if (it != keyValues.end())
			{
				return reinterpret_cast<Field_t*>(&(*it));
			}

			return nullptr;
		}

		std::vector<Field_t> keyValues;
	};

	typedef std::vector<Object_t> Node_t;

	const char* ParseQuoted(const char* const subKeyStart, const char* const subKeyEnd, std::string& quoted);
	const char* ParseKeyValue(const char* const subKeyStart, const char* const subKeyEnd, Object_t& subKey);
	void ParseKeyValues(const char* const subKeyStart, const char* const subKeyEnd, Object_t& subKey);

	bool ParseHeader(const char* const partitionBuffer);

private: // Internal tools;
	bool GetBrushModelFields(Object_t& object, std::vector<Field_t*>& brushModelFields);

	bool EncodeBrushModel(Object_t& object, const std::vector<unsigned char>& brushModelData);
	bool DecodeBrushModel(Object_t& object, std::vector<unsigned char>& brushModelData);
	bool ConvertBrushModel(std::vector<uint8_t>& brushModelData);

public:
	CEntityPartitionMgr() { m_numHeaderFields = 0;  m_entities = -1; m_numModels = -1; }
	bool ParseFromBuffer(const char* const partitionBuffer, const bool parseHeader);
	bool Write(const char* const fileName);
	void WriteToString(std::string& buffer);

	bool ConvertEntityPartition();

private:
	int m_numHeaderFields;
	int m_entities;
	int m_numModels;
	Node_t m_base;
};
