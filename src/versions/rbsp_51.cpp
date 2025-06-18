#include "stdafx.h"
#include "versions.h"
#include "rmem.h"
#include "bspfile.h"

// convert v51 lightprobes to v47
// lightprobe struct got smaller by 4 bytes in version 51 by removing the "pad" variable
// that was used to align the struct to 16 bytes to use SIMD operations for optimisation
// this function appends the bytes back to the struct to make it 16 bytes again
void ConvertLightProbes_v51(rmem& lumpbuf, char*& lumpData, size_t& lumpSize)
{
	const size_t numLightProbes = lumpSize / (sizeof(r5::v51::dlightprobe_t));
	const size_t newLumpSize = numLightProbes * sizeof(dlightprobe_t);

	// allocate buffer for the converted lump data
	char* const newLumpData = new char[newLumpSize];
	rmem newLumpBuf(newLumpData);

	const r5::v51::dlightprobe_t* const lightProbes = reinterpret_cast<r5::v51::dlightprobe_t*>(lumpbuf.getPtr());

	for (size_t j = 0; j < numLightProbes; ++j)
	{
		lumpbuf.seek(j * sizeof(r5::v51::dlightprobe_t), rseekdir::beg);

		newLumpBuf.write<r5::v51::dlightprobe_t>(lightProbes[j]);
		newLumpBuf.write<int>(0);
	}

	delete[] lumpData;

	lumpData = newLumpData;
	lumpSize = newLumpSize;
}
