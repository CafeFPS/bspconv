#pragma once

#include <iostream>
#include <memory>
#include <cassert>
#include <algorithm>
#include <map>
#include <unordered_map>
#include <fstream>

#include "utils.h"

namespace fs = std::filesystem;
#define V_ARRAYSIZE(arr) ((sizeof(arr) / sizeof(*arr)))
