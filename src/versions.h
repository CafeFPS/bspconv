#pragma once
#include "rmem.h"

void ConvertLightProbes_v51(rmem& lumpbuf, char*& lumpData, size_t& lumpSize);
void ConvertBSP(const std::string& bspPath, char* const bspBuf, const bool packAllLumps);
