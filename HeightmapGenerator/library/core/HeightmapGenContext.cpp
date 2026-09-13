#include "HeightmapGenContext.h"
#include "Generators.h"

ContextImpl::~ContextImpl() {
    for (auto* gen : generators) {
        delete gen;
    }
    generators.clear();
}