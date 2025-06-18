#include "stdafx.h"
#include "versions.h"

#include "binstream.h"
#include "stltools.h"

#include "rmem.h"
#include "bspfile.h"
#include "entity_partition.h"

bool FixEntityPartition(const std::string& partitionPath, const bool parseHeader)
{
	CIOStream inEntityPartition;
	if (!inEntityPartition.Open(partitionPath, CIOStream::READ | CIOStream::BINARY))
	{
		printf("%s: Failed to open entity partition file: '%s'\n", __FUNCTION__, partitionPath.c_str());
		return false;
	}

	std::string partitionBuffer;
	inEntityPartition.ReadString(partitionBuffer);

	CEntityPartitionMgr partitionMgr;
	if (!partitionMgr.ParseFromBuffer(partitionBuffer.c_str(), parseHeader))
	{
		printf("%s: Failed to parse entity partition file: '%s'\n", __FUNCTION__, partitionPath.c_str());
		return false;
	}

	const std::string newFile(partitionPath + ".new");

	if (!partitionMgr.ConvertEntityPartition())
	{
		printf("%s: Failed to convert entity partition file: '%s'\n", __FUNCTION__, newFile.c_str());
		return false;
	}

	if (!partitionMgr.Write(newFile.c_str()))
	{
		printf("%s: Failed to write entity partition file: '%s'\n", __FUNCTION__, newFile.c_str());
		return false;
	}

	printf("Writing new entity partition file: %s\n", partitionPath.c_str());
	return true;
}


bool GetEntityPartitionNames(const std::string& bspPath, std::vector<std::string>& vec)
{
	const std::string entityPartitionLump = Format("%s.%.4X.bsp_lump", bspPath.c_str(), lumptype_t::LUMP_ENTITY_PARTITIONS);
	CIOStream read;

	if (!read.Open(entityPartitionLump, CIOStream::READ | CIOStream::BINARY))
	{
		printf("Failed to open entity partition lump\n");
		return false;
	}

	dentitypartitionheader_t ep;
	read.Read(ep);

	if (ep.ident != dentitypartitionheader_t::VERSION)
	{
		printf("Unrecognized header in Entity Partition lump in bsp; ident=%hd, expected=%hd\n",
			ep.ident, dentitypartitionheader_t::VERSION);
		return false;
	}

	/*const bool shouldHaveEntityLump =*/ read.Read<char>() /*== '*'*/;

	std::string str;

	if (read.ReadString(str))
	{
		vec = StringSplit(str, ' ');
		return true;
	}

	return false;
}

// newer versions of the game have an extra field in the BVH header, this field
// has to be removed in order for BVH to function correctly. decode all *coll#
// base64 strings, remove the field, re-encode the data.
//
// NOTE: if additional changes are found or made in the entity partitions,
// such as renamed keys or header changes, perform the conversion here!
void FixEntityPartitions(const std::string& bspPath)
{
	const std::string pathNoExtension = RemoveExtension(bspPath);
	std::vector<std::string> entityPartitionNames;

	if (GetEntityPartitionNames(bspPath, entityPartitionNames))
	{
		for (const std::string& name : entityPartitionNames)
		{
			const std::string partitionPath = Format("%s_%s.ent", pathNoExtension.c_str(), name.c_str());
			FixEntityPartition(partitionPath, true);
		}
	}
}

void WriteNewLump(const std::string& lumpPath, const char* const lumpData, const size_t lumpSize)
{
	const std::string newLumpPath = lumpPath + ".new";

	printf("Writing new lump to: \"%s\" size: %zu\n", newLumpPath.c_str(), lumpSize);

	CIOStream outGameProbes;
	if (outGameProbes.Open(newLumpPath, CIOStream::WRITE | CIOStream::BINARY))
		outGameProbes.Write(lumpData, lumpSize);
	else
		Error("Failed to open file for writing: %s\n", newLumpPath.c_str());
}

// gamelumps have an absolute file offset to their data which needs to be updated to their new file offset
void FixGameLumpOffset(rmem& lumpBuf, const int nextLumpWriteOffset, const bool isPacked)
{
	const int numGameLumps = lumpBuf.read<int>();

	if (numGameLumps != 1)
		Error("Expected 1 game lump but found %i\n", numGameLumps);

	r5::dgamelump_t* pGameLump = lumpBuf.get<r5::dgamelump_t>();

	// adjust file offset to be relative to new location in bsp
	static const int headerOffset = (sizeof(dgamelumpheader_t) + sizeof(r5::dgamelump_t));


	// if the file is not going to be packed, we need to write the offset to the gamelump data, which is
	// just sizeof(dgamelumpheader_t) + sizeof(dgamelump_t), else the packed file offsets needs to be written too
	if (isPacked)
		pGameLump->fileofs = nextLumpWriteOffset + headerOffset;
	else
		pGameLump->fileofs = headerOffset;
}

// expand lightmap RTL data to full size by padding with null bytes
//
// When loaded from .bsp_lump, RTL lightmaps set a bool in CMaterialSystem that is used to change the way that lightmap 
// data is used in shaders. Since this tool combines the .bsp_lumps into one .bsp without adding the missing data that would be
// required from the original internal .bsp lump, that bool is not set and the data is invalid, causing black boxes to appear everywhere
void FixLightmapRTLSize(BSPHeader_t* pHdr, const bool isPacked)
{
	assert(pHdr);

	// we bump the version to 1 so that a check can be made in R5SDK to manually set that bool without having to try and automatically
	// identify invalid lightmap data
	if (isPacked && pHdr)
	{
		pHdr->lumps[LUMP_LIGHTMAP_DATA_REAL_TIME_LIGHTS].version = 1;
		pHdr->lumps[LUMP_LIGHTMAP_DATA_SKY].version = 1;
	}
}

// convert BSP from incompatible versions to version 47.
void ConvertBSP(const std::string& bspPath, char* const bspBuf, const bool packAllLumps)
{
	CIOStream out;
	if (!out.Open(bspPath + ".new", CIOStream::WRITE | CIOStream::BINARY))
		Error("Failed to write output BSP file; insufficient rights?\n");

	if(packAllLumps) // seek to end of header as we write lump data past it
		out.Seek(sizeof(BSPHeader_t));

	rmem buf(bspBuf);
	BSPHeader_t* const pHdr = buf.get<BSPHeader_t>();

	if (pHdr->ident != 'PSBr')
		Error("Input file had invalid magic (expected \"rBSP\")\n");

	// we shouldnt need to access these later so we can get away with modifying
	// these vars here for convenience

	const int currentVersion = pHdr->version;
	pHdr->version = BSPVERSION;
	pHdr->flags = 0;

	if (currentVersion >= 48)
		FixEntityPartitions(bspPath);

	const int numLumps = pHdr->lastLump + 1;
	std::vector<lump_t> lumps(numLumps);

	// copy lump info from header into vector so it can be sorted by offset
	memcpy_s(lumps.data(), numLumps * sizeof(lump_t), &pHdr->lumps, numLumps * sizeof(lump_t));

	// write lump index into uncompLen so that it can be accessed after sorting
	// uncompLen should be unused (and have no existing values from file)
	// leaving this var set isn't a problem because the "lumps" vector isn't written to file
	for (int i = 0; i < lumps.size(); ++i)
	{
		lumps[i].uncompLen = i;
	}

	// sort by lump offset
	std::sort(lumps.begin(), lumps.end());

	int nextLumpWriteOffset = sizeof(BSPHeader_t);

	for (const lump_t& lump : lumps)
	{
		if (lump.filelen == 0)
			continue;

		// retrieve lump index from temp storage in uncompLen
		const int i = lump.uncompLen;

		// e.g. mp_rr_box.bsp.007f.bsp_lump
		const std::string lumpPath = Format("%s.%04x.bsp_lump", bspPath.c_str(), i);

		// make sure the lump file actually exists
		if (!std::filesystem::exists(lumpPath))
		{
			printf("Lump %04x file not found: %s\n", i, lumpPath.c_str());
			continue;
		}

		size_t lumpSize = GetFileSize(lumpPath);

		if (int(lumpSize) != lump.filelen)
			printf("Lump %04x file size mismatch (file %i, bsp %i)\n", i, int(lumpSize), lump.filelen);

		CIOStream lumpIn;

		if (!lumpIn.Open(lumpPath, CIOStream::READ | CIOStream::BINARY))
		{
			printf("Failed to open lump \"%s\"\n", lumpPath.c_str());
			continue;
		}

		char* lumpData = new char[lumpSize];
		lumpIn.Read(lumpData, lumpSize);

		size_t lumpOffset = 0;

		switch (i)
		{
		case LUMP_ENTITIES:
		{
			if (currentVersion >= 48)
			{
				CEntityPartitionMgr epson;
				if (!epson.ParseFromBuffer(lumpData, false))
					printf("%s: Failed to parse \"%s\"\n", __FUNCTION__, "LUMP_ENTITIES");
				else
				{
					if (epson.ConvertEntityPartition())
					{
						std::string outBuf;
						epson.WriteToString(outBuf);

						// Copy into existing buffer; the sizes won't change
						// as the conversion process removes 8 bytes and pads
						// them elsewhere for alignment reasons (as of the RPak
						// v12.1 change).
						outBuf.copy(lumpData, outBuf.size());

						if (!packAllLumps)
							WriteNewLump(lumpPath, lumpData, lumpSize);
					}
					else
					{
						printf("%s: Failed to convert \"%s\"\n", __FUNCTION__, "LUMP_ENTITIES");
						assert(0);
					}
				}
			}

			break;
		}
		case LUMP_GAME_LUMP:
		{
			if (!packAllLumps)
			{
				rmem lumpBuf(lumpData);

				FixGameLumpOffset(lumpBuf, nextLumpWriteOffset, packAllLumps);
				WriteNewLump(lumpPath, lumpData, lumpSize);
			}

			break;
		}
		case LUMP_LIGHTPROBES:
		{
			if (currentVersion >= 51)
			{
				rmem lumpBuf(lumpData);
				ConvertLightProbes_v51(lumpBuf, lumpData, lumpSize);

				if (!packAllLumps)
					WriteNewLump(lumpPath, lumpData, lumpSize);
			}

			break;
		}
		case LUMP_LIGHTMAP_DATA_REAL_TIME_LIGHTS:
		{
			FixLightmapRTLSize(pHdr, packAllLumps);
			lumpSize = lump.filelen;

			break;
		}
		}

		pHdr->lumps[i].fileofs = int(lumpOffset);
		pHdr->lumps[i].filelen = int(lumpSize);

		if (packAllLumps)
		{
			pHdr->lumps[i].fileofs = nextLumpWriteOffset;
			out.Write(lumpData, lumpSize);
			nextLumpWriteOffset += int(lumpSize);
		}

		delete[] lumpData;
	}

	// seek back to write the header
	if (packAllLumps)
		out.Seek(0);

	out.Write(pHdr, sizeof(BSPHeader_t));
}
