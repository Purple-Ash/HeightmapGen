#pragma once
#include "HeightmapGenAPI.h"

using PFN_smokeTest = const char*(const char*);

// context lifecycle functions
using PFN_createContext = Context();
using PFN_destroyContext = void(Context);

// generator lifecycle functions
using PFN_createCoordinateGenerator = Generator(Context, CommonSettings);
using PFN_createRandomGenerator = Generator(Context, CommonSettings);
using PFN_createVoronoiGenerator = Generator(Context, CommonSettings);
using PFN_createHydraulicErosionGenerator = Generator(Context, CommonSettings, HydraulicErosionSettings);
using PFN_createPerlinGenerator = Generator(Context, CommonSettings);
using PFN_createBrownianPerlinGenerator = Generator(Context, CommonSettings);
using PFN_destroyGenerator = void(Generator);

// generator usage functions
using PFN_getChunk = void(Generator, int32_t x, int32_t y, float* buffer);
using PFN_getPoint = float(Generator, int32_t x, int32_t y);
using PFN_requestChunk = void(Generator, int32_t x, int32_t y);
using PFN_requestPoint = void(Generator, int32_t x, int32_t y);
using PFN_probeChunk = bool(Generator, int32_t x, int32_t y, float* buffer);
using PFN_probePoint = bool(Generator, int32_t x, int32_t y, float* point);
using PFN_cleanChunkFromCache = void(Generator, int32_t x, int32_t y);
using PFN_clearAllCache = void(Generator);