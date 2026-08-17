#include "HeightmapGenContext.h"
#include "Generators.h"

ContextImpl::~ContextImpl() {
    for (auto* gen : GeneratorImpls) {
        gen->context = nullptr; 
        delete gen;
    }
    GeneratorImpls.clear();
}