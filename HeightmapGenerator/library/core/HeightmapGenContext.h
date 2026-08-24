#pragma once
#include "HeightmapGenAPI.h"
#include "Helpers.h"

namespace std {
    template<> struct hash<Vec2Int> {
        size_t operator()(const Vec2Int& c) const noexcept {
            size_t h1 = hash<int32_t>{}(c.x);
            size_t h2 = hash<int32_t>{}(c.y);
            return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
        }
    };
}

struct ContextImpl {
    std::vector<GeneratorHandle> GeneratorImpls;
    ~ContextImpl();
};