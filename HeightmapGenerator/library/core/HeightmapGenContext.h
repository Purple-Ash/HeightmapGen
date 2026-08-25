#pragma once
#include "HeightmapGenAPI.h"
#include "Helpers.h"

struct ContextImpl {
    std::vector<Generator> generators;
    ~ContextImpl();
};