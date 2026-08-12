#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include "HeightmapGenContext.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace {
    constexpr std::int32_t kVoptixChunkEdge = 128;
    constexpr std::int32_t kPaletteEdge = 8;
    constexpr std::int32_t kPaletteSlots = 256;
    constexpr std::int32_t kPaletteVariants = 8;
    constexpr std::int32_t kPalettePixelsPerVoxel = 2;
    constexpr std::int32_t kPaletteCapacity = kPaletteSlots * kPaletteVariants;
    constexpr float kDefaultAmplitude = 32.0F;
    constexpr std::array<std::uint8_t, 3> kGrassColor{ 76, 138, 46 };
    constexpr std::array<std::uint8_t, 3> kDirtColor{ 122, 78, 43 };
    constexpr std::array<std::uint8_t, 3> kBedrockColor{ 94, 96, 100 };
    constexpr std::array<std::uint8_t, 3> kSingleTerrainColor{ 93, 128, 61 };

    enum class FallbackMaterial : std::int32_t { grass = 0, dirt = 1, bedrock = 2 };

    struct Options {
        std::string outputPath;
        std::int32_t regionsX = 3;
        std::int32_t regionsZ = 3;
        std::int32_t originX = 19;
        std::int32_t originZ = 19;
        float amplitude = kDefaultAmplitude;
        float horizontalScale = 0.02F;
        GeneratorType generator = GeneratorType::random;
        HydraulicErosionSettings erosion;
        std::int32_t subdivision = 2;
        std::int32_t subdivisionUp = 2;
        std::string palettePath;
        std::string dllPath;
        bool singleColor = false;
    };

    class HeightmapLibrary {
    public:
        ~HeightmapLibrary() {
            if (module_ != nullptr) FreeLibrary(module_);
        }

        bool load(const std::filesystem::path& path) {
            module_ = LoadLibraryW(path.c_str());
            if (module_ == nullptr) return false;
            getNewContext_ = resolve<GetNewContext>("getNewContext");
            closeContext_ = resolve<CloseContext>("closeContext");
            setRegionDimensions_ = resolve<SetRegionDimensions>("setRegionDimensions");
            setVerticalAmplitude_ = resolve<SetVerticalAmplitude>("setVerticalAmplitude");
            setResolution_ = resolve<SetResolution>("setResolution");
            setGenerator_ = resolve<SetGenerator>("setGenerator");
            generateRegion_ = resolve<GenerateRegion>("generateRegion");
            getRegion_ = resolve<GetRegion>("getRegion");
            return getNewContext_ && closeContext_ && setRegionDimensions_ && setVerticalAmplitude_
                && setResolution_ && setGenerator_ && generateRegion_ && getRegion_;
        }

        HGContextHandle getNewContext() const { return getNewContext_(); }
        bool closeContext(HGContextHandle context) const { return closeContext_(context); }
        bool setRegionDimensions(HGContextHandle context, std::int32_t sizeX, std::int32_t sizeZ) const {
            return setRegionDimensions_ != nullptr && setRegionDimensions_(context, sizeX, sizeZ);
        }
        bool setVerticalAmplitude(HGContextHandle context, float amplitude) const {
            return setVerticalAmplitude_(context, amplitude);
        }
        bool setResolution(HGContextHandle context, std::int32_t resolution) const {
            return setResolution_(context, resolution);
        }
        bool setGenerator(HGContextHandle context, GeneratorType generator, float scale, void* settings) const {
            return setGenerator_(context, generator, scale, settings);
        }
        bool generateRegion(HGContextHandle context, std::int32_t x, std::int32_t z) const {
            return generateRegion_(context, x, z);
        }
        bool getRegionSamples(HGContextHandle context, std::int32_t x, std::int32_t z,
            const float*& samples, std::int32_t& sampleCountX,
            std::int32_t& sampleCountZ) const {
            Chunk* region = nullptr;
            if (!getRegion_(context, x, z, region) || region == nullptr || region->data == nullptr) return false;
            // The export uses resolution 1, so dimensions map directly to sample count plus 1.
            samples = region->data;
            sampleCountX = region->size.x + 1;
            sampleCountZ = region->size.y + 1;
            return true;
        }

    private:
        using GetNewContext = HGContextHandle(*)();
        using CloseContext = bool (*)(HGContextHandle);
        using SetRegionDimensions = bool (*)(HGContextHandle, std::int32_t, std::int32_t);
        using SetVerticalAmplitude = bool (*)(HGContextHandle, float);
        using SetResolution = bool (*)(HGContextHandle, std::int32_t);
        using SetGenerator = bool (*)(HGContextHandle, GeneratorType, float, void*);
        using GenerateRegion = bool (*)(HGContextHandle, std::int32_t, std::int32_t);
        using GetRegion = bool (*)(HGContextHandle, std::int32_t, std::int32_t, Chunk*&);

        template <typename Function>
        Function resolve(const char* name) const {
            return reinterpret_cast<Function>(GetProcAddress(module_, name));
        }

        HMODULE module_ = nullptr;
        GetNewContext getNewContext_ = nullptr;
        CloseContext closeContext_ = nullptr;
        SetRegionDimensions setRegionDimensions_ = nullptr;
        SetVerticalAmplitude setVerticalAmplitude_ = nullptr;
        SetResolution setResolution_ = nullptr;
        SetGenerator setGenerator_ = nullptr;
        GenerateRegion generateRegion_ = nullptr;
        GetRegion getRegion_ = nullptr;
    };

    std::filesystem::path defaultDllPath() {
        std::array<wchar_t, MAX_PATH> executablePath{};
        const auto length = GetModuleFileNameW(nullptr, executablePath.data(), static_cast<DWORD>(executablePath.size()));
        if (length == 0 || length == executablePath.size()) return {};
        return std::filesystem::path(executablePath.data()).parent_path() / L"HeightmapGen.dll";
    }

    void printUsage(const char* executable) {
        std::cerr << "Usage: " << executable << " <output.vx> [options]\n"
            << "  --regions-x N          Number of chunks along X (default: 3)\n"
            << "  --regions-z N          Number of chunks along Z (default: 3)\n"
            << "  --origin-x N           First Voptix chunk X coordinate (default: 19)\n"
            << "  --origin-z N           First Voptix chunk Z coordinate (default: 19)\n"
            << "  --amplitude N          Terrain height in voxels, 1..128 (default: 32)\n"
            << "  --horizontal-scale N   Heightmap noise frequency (default: 0.02)\n"
            << "  --generator NAME       random, perlin, brownian, or hydraulic (default: brownian)\n"
            << "  --erosion-seed N       Hydraulic droplet seed (default: 0)\n"
            << "  --erosion-iterations N Hydraulic droplets per terrain cell (default: 10)\n"
            << "  --erosion-radius N     Hydraulic brush radius in samples (default: 3)\n"
            << "  --erosion-lifetime N   Maximum steps per droplet (default: 30)\n"
            << "  --erosion-inertia N    Droplet inertia, 0..1 (default: 0.05)\n"
            << "  --erosion-capacity N   Sediment capacity factor (default: 4)\n"
            << "  --erosion-min-capacity N Minimum sediment capacity (default: 0.01)\n"
            << "  --erosion-erode-speed N Erosion rate, 0..1 (default: 0.3)\n"
            << "  --erosion-deposit-speed N Deposition rate, 0..1 (default: 0.3)\n"
            << "  --erosion-evaporation N Evaporation rate, 0..1 (default: 0.01)\n"
            << "  --erosion-gravity N    Gravity applied to droplets (default: 4)\n"
            << "  --erosion-start-speed N Initial droplet speed (default: 1)\n"
            << "  --erosion-start-water N Initial droplet water (default: 1)\n"
            << "  --subdivision N        Terrain cells along X/Z and, unless overridden, Y: 1, 2, 3, 4, or 8 (default: 2)\n"
            << "  --subdivision-up N     Terrain cells along the up/Y axis: 1, 2, 3, 4, or 8 (default: --subdivision)\n"
            << "  --palette PATH         Palette output path (default: output name with .vp)\n"
            << "  --dll PATH             Existing HeightmapGen DLL (default: HeightmapGen.dll beside this executable)\n"
            << "  --single-color         Use one colour for all terrain blocks to reuse more variants\n";
    }

    bool parseInt(std::string_view text, std::int32_t& value) {
        try {
            std::size_t parsed = 0;
            const auto number = std::stoll(std::string(text), &parsed);
            if (parsed != text.size() || number < std::numeric_limits<std::int32_t>::min()
                || number > std::numeric_limits<std::int32_t>::max()) return false;
            value = static_cast<std::int32_t>(number);
            return true;
        }
        catch (...) { return false; }
    }

    bool parseFloat(std::string_view text, float& value) {
        try {
            std::size_t parsed = 0;
            value = std::stof(std::string(text), &parsed);
            return parsed == text.size() && std::isfinite(value);
        }
        catch (...) { return false; }
    }

    bool parseArguments(int argc, char** argv, Options& options) {
        if (argc < 2) return false;
        options.outputPath = argv[1];
        bool subdivisionUpExplicit = false;

        for (int index = 2; index < argc;) {
            const std::string_view name = argv[index++];
            if (name == "--single-color") {
                options.singleColor = true;
                continue;
            }
            if (index >= argc) return false;
            const std::string_view value = argv[index++];
            if (name == "--regions-x") {
                if (!parseInt(value, options.regionsX)) return false;
            }
            else if (name == "--regions-z") {
                if (!parseInt(value, options.regionsZ)) return false;
            }
            else if (name == "--origin-x") {
                if (!parseInt(value, options.originX)) return false;
            }
            else if (name == "--origin-z") {
                if (!parseInt(value, options.originZ)) return false;
            }
            else if (name == "--amplitude") {
                if (!parseFloat(value, options.amplitude)) return false;
            }
            else if (name == "--horizontal-scale") {
                if (!parseFloat(value, options.horizontalScale)) return false;
            }
            else if (name == "--generator") {
                if (value == "random") options.generator = GeneratorType::random;
                else if (value == "perlin") options.generator = GeneratorType::perlin;
                else if (value == "brownian") options.generator = GeneratorType::brownian_perlin;
                else if (value == "hydraulic" || value == "hydraulic-erosion" || value == "erosion")
                    options.generator = GeneratorType::hydraulic_erosion;
                else if (value == "coordinate") options.generator = GeneratorType::coordinate;
                else return false;
            }
            else if (name == "--erosion-seed") {
                if (!parseInt(value, options.erosion.seed)) return false;
            }
            else if (name == "--erosion-iterations") {
                if (!parseInt(value, options.erosion.numIterations)) return false;
            }
            else if (name == "--erosion-radius") {
                if (!parseInt(value, options.erosion.erosionRadius)) return false;
            }
            else if (name == "--erosion-lifetime") {
                if (!parseInt(value, options.erosion.maxDropletLifetime)) return false;
            }
            else if (name == "--erosion-inertia") {
                if (!parseFloat(value, options.erosion.inertia)) return false;
            }
            else if (name == "--erosion-capacity") {
                if (!parseFloat(value, options.erosion.sedimentCapacityFactor)) return false;
            }
            else if (name == "--erosion-min-capacity") {
                if (!parseFloat(value, options.erosion.minSedimentCapacity)) return false;
            }
            else if (name == "--erosion-erode-speed") {
                if (!parseFloat(value, options.erosion.erodeSpeed)) return false;
            }
            else if (name == "--erosion-deposit-speed") {
                if (!parseFloat(value, options.erosion.depositSpeed)) return false;
            }
            else if (name == "--erosion-evaporation") {
                if (!parseFloat(value, options.erosion.evaporateSpeed)) return false;
            }
            else if (name == "--erosion-gravity") {
                if (!parseFloat(value, options.erosion.gravity)) return false;
            }
            else if (name == "--erosion-start-speed") {
                if (!parseFloat(value, options.erosion.initialSpeed)) return false;
            }
            else if (name == "--erosion-start-water") {
                if (!parseFloat(value, options.erosion.initialWaterVolume)) return false;
            }
            else if (name == "--subdivision") {
                if (!parseInt(value, options.subdivision)) return false;
                if (!subdivisionUpExplicit) options.subdivisionUp = options.subdivision;
            }
            else if (name == "--subdivision-up") {
                if (!parseInt(value, options.subdivisionUp)) return false;
                subdivisionUpExplicit = true;
            }
            else if (name == "--palette") {
                options.palettePath = value;
            }
            else if (name == "--dll") {
                options.dllPath = value;
            }
            else return false;
        }

        const auto lastX = static_cast<std::uint64_t>(options.originX) + options.regionsX - 1;
        const auto lastZ = static_cast<std::uint64_t>(options.originZ) + options.regionsZ - 1;
        return options.regionsX > 0 && options.regionsZ > 0 && options.originX >= 0 && options.originZ >= 0
            && lastX <= std::numeric_limits<std::uint32_t>::max()
            && lastZ <= std::numeric_limits<std::uint32_t>::max() && options.amplitude > 0.0F
            && options.amplitude <= static_cast<float>(kVoptixChunkEdge)
            && options.horizontalScale > 0.0F
            && (options.subdivision == 1 || options.subdivision == 2 || options.subdivision == 3
                || options.subdivision == 4 || options.subdivision == 8)
            && (options.subdivisionUp == 1 || options.subdivisionUp == 2 || options.subdivisionUp == 3
                || options.subdivisionUp == 4 || options.subdivisionUp == 8)
            && options.erosion.numIterations >= 0
            && options.erosion.erosionRadius > 0
            && options.erosion.maxDropletLifetime > 0
            && options.erosion.inertia >= 0.0F && options.erosion.inertia <= 1.0F
            && options.erosion.sedimentCapacityFactor >= 0.0F
            && options.erosion.minSedimentCapacity >= 0.0F
            && options.erosion.erodeSpeed >= 0.0F && options.erosion.erodeSpeed <= 1.0F
            && options.erosion.depositSpeed >= 0.0F && options.erosion.depositSpeed <= 1.0F
            && options.erosion.evaporateSpeed >= 0.0F && options.erosion.evaporateSpeed <= 1.0F
            && options.erosion.gravity >= 0.0F
            && options.erosion.initialSpeed >= 0.0F
            && options.erosion.initialWaterVolume >= 0.0F;
    }

    void writeUint32LittleEndian(std::ofstream& output, std::uint32_t value) {
        const std::uint8_t bytes[4] = {
            static_cast<std::uint8_t>(value), static_cast<std::uint8_t>(value >> 8),
            static_cast<std::uint8_t>(value >> 16), static_cast<std::uint8_t>(value >> 24) };
        output.write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
    }

    struct Pattern {
        std::int32_t subdivision;
        std::int32_t subdivisionUp;
        std::array<std::uint8_t, kPaletteEdge* kPaletteEdge* kPaletteEdge * 4> cells;
    };

    enum class PatternKind : std::uint8_t { detailed, fullGrass, fullDirt, fullBedrock };

    std::array<std::uint8_t, 3> colorFor(FallbackMaterial material, bool singleColor) {
        if (singleColor) return kSingleTerrainColor;
        switch (material) {
            case FallbackMaterial::grass: return kGrassColor;
            case FallbackMaterial::bedrock: return kBedrockColor;
            default: return kDirtColor;
        }
    }

    Pattern fullBlock(FallbackMaterial material, bool singleColor) {
        Pattern pattern{};
        pattern.subdivision = 1;
        pattern.subdivisionUp = 1;
        const auto color = colorFor(material, singleColor);
        pattern.cells[0] = color[0];
        pattern.cells[1] = color[1];
        pattern.cells[2] = color[2];
        pattern.cells[3] = 255;
        return pattern;
    }

    struct PatternSample {
        Pattern pattern;
        PatternKind kind = PatternKind::detailed;
        FallbackMaterial fallback = FallbackMaterial::dirt;
        bool occupied = false;
    };

    // Maps a logical subdivision cell onto the palette's fixed 8-cell edge.
    // The 3-way mode deliberately uses widths 3, 2, 3.
    std::int32_t subdivisionCellBegin(std::int32_t subdivision, std::int32_t cell) {
        if (subdivision == 3) {
            constexpr std::array<std::int32_t, 3> begins{ 0, 3, 5 };
            return begins[static_cast<std::size_t>(cell)];
        }
        return cell * (kPaletteEdge / subdivision);
    }

    std::int32_t subdivisionCellSize(std::int32_t subdivision, std::int32_t cell) {
        if (subdivision == 3) {
            constexpr std::array<std::int32_t, 3> sizes{ 3, 2, 3 };
            return sizes[static_cast<std::size_t>(cell)];
        }
        return kPaletteEdge / subdivision;
    }

    float subdivisionCellCenter(std::int32_t subdivision, std::int32_t cell) {
        return (static_cast<float>(subdivisionCellBegin(subdivision, cell))
            + 0.5F * subdivisionCellSize(subdivision, cell)) / kPaletteEdge;
    }

    float subdivisionCellThickness(std::int32_t subdivision, std::int32_t cell) {
        return static_cast<float>(subdivisionCellSize(subdivision, cell)) / kPaletteEdge;
    }

    PatternSample makePattern(float h00, float h10, float h01, float h11, std::int32_t blockY,
        std::int32_t subdivision, std::int32_t subdivisionUp, bool singleColor) {
        PatternSample result;
        const float minHeight = std::min({ h00, h10, h01, h11 });
        if (blockY == 0 && minHeight >= 1.0F) {
            result.kind = PatternKind::fullBedrock;
            result.fallback = FallbackMaterial::bedrock;
            result.occupied = true;
            return result;
        }
        if (blockY + 1.0F <= minHeight - 1.0F) {
            result.kind = PatternKind::fullDirt;
            result.occupied = true;
            return result;
        }

        result.pattern.subdivision = subdivision;
        result.pattern.subdivisionUp = subdivisionUp;
        std::fill_n(result.pattern.cells.begin(), static_cast<std::size_t>(subdivision) * subdivisionUp * subdivision * 4, 0);
        bool hasGrass = false;
        for (std::int32_t z = 0; z < subdivision; ++z) {
            const float tz = subdivisionCellCenter(subdivision, z);
            for (std::int32_t x = 0; x < subdivision; ++x) {
                const float tx = subdivisionCellCenter(subdivision, x);
                const float height = std::lerp(std::lerp(h00, h10, tx), std::lerp(h01, h11, tx), tz);
                for (std::int32_t y = 0; y < subdivisionUp; ++y) {
                    const float worldY = blockY + subdivisionCellCenter(subdivisionUp, y);
                    if (worldY >= height) continue;

                    const bool grass = blockY > 0
                        && height - worldY <= subdivisionCellThickness(subdivisionUp, y);
                    const auto color = singleColor ? kSingleTerrainColor : (grass ? kGrassColor : kDirtColor);
                    hasGrass = hasGrass || grass;
                    result.occupied = true;
                    const auto offset = static_cast<std::size_t>(x + (y + z * subdivisionUp) * subdivision) * 4;
                    result.pattern.cells[offset] = color[0];
                    result.pattern.cells[offset + 1] = color[1];
                    result.pattern.cells[offset + 2] = color[2];
                    result.pattern.cells[offset + 3] = 255;
                }
            }
        }
        result.fallback = hasGrass ? FallbackMaterial::grass : FallbackMaterial::dirt;
        return result;
    }

    std::string patternKey(const Pattern& pattern) {
        const auto byteCount = static_cast<std::size_t>(pattern.subdivision) * pattern.subdivisionUp * pattern.subdivision * 4;
        std::string key(2 + byteCount, '\0');
        key[0] = static_cast<char>(pattern.subdivision);
        key[1] = static_cast<char>(pattern.subdivisionUp);
        std::copy_n(reinterpret_cast<const char*>(pattern.cells.data()), byteCount, key.data() + 2);
        return key;
    }

    class PatternRegistry {
    public:
        static constexpr std::int32_t kNormalCapacity = kPaletteCapacity - 3;

        explicit PatternRegistry(bool singleColor) : singleColor_(singleColor) {
            addFull(FallbackMaterial::bedrock);
            addFull(FallbackMaterial::dirt);
            if (singleColor_) addFull(FallbackMaterial::grass);
            else fullIndices_[static_cast<std::size_t>(FallbackMaterial::grass)] = kNormalCapacity;
        }

        void add(const PatternSample& sample) {
            if (!sample.occupied) return;
            if (sample.kind != PatternKind::detailed) return;
            const auto key = patternKey(sample.pattern);
            if (patterns_.contains(key)) return;
            if (static_cast<std::int32_t>(ordered_.size()) < kNormalCapacity) {
                patterns_.emplace(key, static_cast<std::int32_t>(ordered_.size()));
                ordered_.push_back(sample.pattern);
            }
            else {
                overflow_ = true;
                ++overflowedVariants_;
                fallbackUsed_[static_cast<std::size_t>(fallbackFor(sample))] = true;
            }
        }

        [[nodiscard]] std::int32_t lookup(const PatternSample& sample) const {
            switch (sample.kind) {
                case PatternKind::fullBedrock: return fullIndices_[static_cast<std::size_t>(FallbackMaterial::bedrock)];
                case PatternKind::fullDirt: return fullIndices_[static_cast<std::size_t>(FallbackMaterial::dirt)];
                case PatternKind::fullGrass: return fullIndices_[static_cast<std::size_t>(FallbackMaterial::grass)];
                default: break;
            }
            const auto it = patterns_.find(patternKey(sample.pattern));
            if (it != patterns_.end()) return it->second;
            return kNormalCapacity + static_cast<std::int32_t>(fallbackFor(sample));
        }

        [[nodiscard]] const std::vector<Pattern>& patterns() const { return ordered_; }
        [[nodiscard]] bool overflowed() const { return overflow_; }
        [[nodiscard]] std::int32_t overflowedVariants() const { return overflowedVariants_; }
        [[nodiscard]] bool fallbackUsed(FallbackMaterial material) const {
            return fallbackUsed_[static_cast<std::size_t>(material)];
        }

    private:
        void addFull(FallbackMaterial material) {
            const auto pattern = fullBlock(material, singleColor_);
            const auto key = patternKey(pattern);
            const auto [it, inserted] = patterns_.emplace(key, static_cast<std::int32_t>(ordered_.size()));
            if (inserted) ordered_.push_back(pattern);
            fullIndices_[static_cast<std::size_t>(material)] = it->second;
        }

        [[nodiscard]] FallbackMaterial fallbackFor(const PatternSample& sample) const {
            return singleColor_ ? FallbackMaterial::grass : sample.fallback;
        }

        std::unordered_map<std::string, std::int32_t> patterns_;
        std::vector<Pattern> ordered_;
        std::array<std::int32_t, 3> fullIndices_{};
        bool singleColor_ = false;
        bool overflow_ = false;
        std::int32_t overflowedVariants_ = 0;
        std::array<bool, 3> fallbackUsed_{};
    };

    template <typename Callback>
    bool forEachBlock(const float* heights, std::int32_t sampleCountX, std::int32_t sampleCountZ,
        std::int32_t subdivision, std::int32_t subdivisionUp, bool singleColor, Callback&& callback) {
        if (sampleCountX != kVoptixChunkEdge + 1 || sampleCountZ != kVoptixChunkEdge + 1) return false;
        for (std::int32_t z = 0; z < kVoptixChunkEdge; ++z) {
            for (std::int32_t x = 0; x < kVoptixChunkEdge; ++x) {
                const float h00 = heights[x * sampleCountZ + z];
                const float h10 = heights[(x + 1) * sampleCountZ + z];
                const float h01 = heights[x * sampleCountZ + z + 1];
                const float h11 = heights[(x + 1) * sampleCountZ + z + 1];
                const auto top = std::clamp(static_cast<std::int32_t>(std::ceil(std::max({ h00, h10, h01, h11 }))) - 1,
                    0, kVoptixChunkEdge - 1);
                for (std::int32_t y = 0; y <= top; ++y) {
                    const auto sample = makePattern(h00, h10, h01, h11, y, subdivision, subdivisionUp, singleColor);
                    if (sample.occupied) callback(x, y, z, sample);
                }
            }
        }
        return true;
    }

    class PaletteWriter {
    public:
        PaletteWriter() {
            for (std::int32_t level = 0, edge = kPaletteEdge; level < 4; ++level, edge /= 2) {
                levels_[level].assign(static_cast<std::size_t>(edge) * edge * edge * 4
                    * kPaletteSlots * kPalettePixelsPerVoxel * kPaletteVariants, 0);
            }
        }

        void addPattern(const Pattern& pattern, std::int32_t paletteIndex) {
            const auto slot = paletteIndex / kPaletteVariants;
            const auto variant = paletteIndex % kPaletteVariants;
            for (std::int32_t z = 0; z < pattern.subdivision; ++z)
                for (std::int32_t y = 0; y < pattern.subdivisionUp; ++y)
                    for (std::int32_t x = 0; x < pattern.subdivision; ++x) {
                        const auto offset = static_cast<std::size_t>(x + (y + z * pattern.subdivisionUp) * pattern.subdivision) * 4;
                        if (pattern.cells[offset + 3] == 0) continue;
                        const auto beginX = subdivisionCellBegin(pattern.subdivision, x);
                        const auto beginY = subdivisionCellBegin(pattern.subdivisionUp, y);
                        const auto beginZ = subdivisionCellBegin(pattern.subdivision, z);
                        const auto sizeX = subdivisionCellSize(pattern.subdivision, x);
                        const auto sizeY = subdivisionCellSize(pattern.subdivisionUp, y);
                        const auto sizeZ = subdivisionCellSize(pattern.subdivision, z);
                        for (std::int32_t dz = 0; dz < sizeZ; ++dz)
                            for (std::int32_t dy = 0; dy < sizeY; ++dy)
                                for (std::int32_t dx = 0; dx < sizeX; ++dx)
                                    octreeSet(beginX + dx, beginY + dy, beginZ + dz,
                                        pattern.cells[offset], pattern.cells[offset + 1], pattern.cells[offset + 2],
                                        slot, variant);
                    }
        }

        bool write(const std::string& path) const {
            std::ofstream output(path, std::ios::binary | std::ios::trunc);
            if (!output) return false;
            writeUint32LittleEndian(output, kPaletteSlots);
            writeUint32LittleEndian(output, kPalettePixelsPerVoxel);
            writeUint32LittleEndian(output, kPaletteVariants);
            writeUint32LittleEndian(output, kPaletteEdge);
            for (const auto& level : levels_)
                output.write(reinterpret_cast<const char*>(level.data()), static_cast<std::streamsize>(level.size()));
            return static_cast<bool>(output);
        }

    private:
        void setLeaf(std::int32_t x, std::int32_t y, std::int32_t z, std::uint8_t r, std::uint8_t g,
            std::uint8_t b, std::int32_t slot, std::int32_t variant) {
            x += slot * kPaletteEdge;
            z += variant * kPaletteEdge;
            const auto offset = static_cast<std::size_t>(x + (y * kPaletteEdge
                + z * kPaletteEdge * kPaletteEdge * kPalettePixelsPerVoxel) * kPaletteSlots) * 4;
            levels_[0][offset] = r;
            levels_[0][offset + 1] = g;
            levels_[0][offset + 2] = b;
            levels_[0][offset + 3] = 255;
        }

        void octreeSet(std::int32_t x, std::int32_t y, std::int32_t z, std::uint8_t r, std::uint8_t g,
            std::uint8_t b, std::int32_t slot, std::int32_t variant) {
            std::int32_t pow2 = 1;
            for (std::int32_t depth = 0; depth < 3; ++depth) {
                const auto xo = x >> (3 - depth);
                const auto yo = (y >> (3 - depth)) * pow2;
                const auto zo = (z >> (3 - depth)) * pow2 * pow2;
                const auto variantOffset = pow2 * pow2 * pow2 * variant * kPalettePixelsPerVoxel;
                const auto index = static_cast<std::size_t>((xo + slot * pow2
                    + (yo + zo * kPalettePixelsPerVoxel + variantOffset) * kPaletteSlots) * 4);
                const auto materialOffset = pow2 * pow2 * kPaletteSlots * 4;
                const auto octant = ((x >> (2 - depth)) & 1) + (((y >> (2 - depth)) & 1) << 1)
                    + (((z >> (2 - depth)) & 1) << 2);
                auto& level = levels_[3 - depth];
                level[index] = r;
                level[index + 1] = g;
                level[index + 2] = b;
                level[index + 3] |= static_cast<std::uint8_t>(1 << octant);
                level[index + materialOffset] = 0;
                level[index + materialOffset + 1] = 0;
                level[index + materialOffset + 2] = 0;
                pow2 *= 2;
            }
            setLeaf(x, y, z, r, g, b, slot, variant);
        }

        std::array<std::vector<std::uint8_t>, 4> levels_;
    };
} // namespace

int main(int argc, char** argv) {
    Options options;
    if (!parseArguments(argc, argv, options)) {
        printUsage(argv[0]);
        return EXIT_FAILURE;
    }

    const auto dllPath = options.dllPath.empty() ? defaultDllPath() : std::filesystem::path(options.dllPath);
    HeightmapLibrary library;
    if (dllPath.empty() || !library.load(dllPath)) {
        std::cerr << "Could not load HeightmapGen DLL from " << dllPath.string() << ".\n";
        return EXIT_FAILURE;
    }

    HGContextHandle context = library.getNewContext();
    if (context == nullptr) {
        std::cerr << "Could not create a HeightmapGen context.\n";
        return EXIT_FAILURE;
    }

    void* const generatorSettings = options.generator == GeneratorType::hydraulic_erosion
        ? static_cast<void*>(&options.erosion) : nullptr;
    if (!library.setRegionDimensions(context, kVoptixChunkEdge, kVoptixChunkEdge)
        || !library.setVerticalAmplitude(context, options.amplitude)
        || !library.setResolution(context, 1)
        || !library.setGenerator(context, options.generator, options.horizontalScale, generatorSettings)) {
        std::cerr << "Could not configure HeightmapGen with the selected DLL.\n";
        library.closeContext(context);
        return EXIT_FAILURE;
    }

    const auto chunkCount = static_cast<std::uint64_t>(options.regionsX) * options.regionsZ;
    if (chunkCount > std::numeric_limits<std::uint32_t>::max()) {
        std::cerr << "Too many chunks.\n";
        library.closeContext(context);
        return EXIT_FAILURE;
    }

    std::vector<std::vector<float>> heightFields(static_cast<std::size_t>(chunkCount));
    for (std::int32_t regionZ = 0; regionZ < options.regionsZ; ++regionZ) {
        for (std::int32_t regionX = 0; regionX < options.regionsX; ++regionX) {
            const float* samples = nullptr;
            std::int32_t sampleCountX = 0;
            std::int32_t sampleCountZ = 0;
            if (!library.generateRegion(context, regionX, regionZ)
                || !library.getRegionSamples(context, regionX, regionZ, samples, sampleCountX, sampleCountZ)
                || sampleCountX != kVoptixChunkEdge + 1 || sampleCountZ != kVoptixChunkEdge + 1) {
                std::cerr << "Could not read the " << kVoptixChunkEdge << "x" << kVoptixChunkEdge
                    << " HeightmapGen region " << regionX << ", " << regionZ << ".\n";
                library.closeContext(context);
                return EXIT_FAILURE;
            }
            auto& field = heightFields[static_cast<std::size_t>(regionX + regionZ * options.regionsX)];
            field.assign(samples, samples + static_cast<std::size_t>(sampleCountX) * sampleCountZ);
        }
    }

    PatternRegistry registry(options.singleColor);
    for (std::int32_t regionZ = 0; regionZ < options.regionsZ; ++regionZ) {
        for (std::int32_t regionX = 0; regionX < options.regionsX; ++regionX) {
            const auto& field = heightFields[static_cast<std::size_t>(regionX + regionZ * options.regionsX)];
            if (!forEachBlock(field.data(), kVoptixChunkEdge + 1, kVoptixChunkEdge + 1,
                options.subdivision, options.subdivisionUp, options.singleColor,
                [&registry](std::int32_t, std::int32_t, std::int32_t, const PatternSample& sample) {
                    registry.add(sample);
                })) {
                std::cerr << "Could not collect palette variants for region " << regionX << ", " << regionZ << ".\n";
                library.closeContext(context);
                return EXIT_FAILURE;
            }
        }
    }

    std::ofstream output(options.outputPath, std::ios::binary | std::ios::trunc);
    if (!output) {
        std::cerr << "Could not create " << options.outputPath << ".\n";
        library.closeContext(context);
        return EXIT_FAILURE;
    }
    writeUint32LittleEndian(output, static_cast<std::uint32_t>(chunkCount));

    // Voptix reads every chunk coordinate first, then the equally ordered raw
    // voxel buffers. Keep these loops separate to preserve that binary layout.
    for (std::int32_t regionZ = 0; regionZ < options.regionsZ; ++regionZ) {
        for (std::int32_t regionX = 0; regionX < options.regionsX; ++regionX) {
            writeUint32LittleEndian(output, static_cast<std::uint32_t>(
                static_cast<std::uint64_t>(options.originX) + regionX));
            writeUint32LittleEndian(output, 0);
            writeUint32LittleEndian(output, static_cast<std::uint32_t>(
                static_cast<std::uint64_t>(options.originZ) + regionZ));
        }
    }

    for (std::int32_t regionZ = 0; regionZ < options.regionsZ; ++regionZ) {
        for (std::int32_t regionX = 0; regionX < options.regionsX; ++regionX) {
            const auto& field = heightFields[static_cast<std::size_t>(regionX + regionZ * options.regionsX)];
            std::vector<std::uint8_t> voxels(static_cast<std::size_t>(kVoptixChunkEdge)
                * kVoptixChunkEdge * kVoptixChunkEdge * 4, 0);
            if (!forEachBlock(field.data(), kVoptixChunkEdge + 1, kVoptixChunkEdge + 1,
                options.subdivision, options.subdivisionUp, options.singleColor,
                [&voxels, &registry](std::int32_t x, std::int32_t y, std::int32_t z,
                    const PatternSample& sample) {
                        const auto paletteIndex = registry.lookup(sample);
                        const auto offset = static_cast<std::size_t>(x + (y + z * kVoptixChunkEdge)
                            * kVoptixChunkEdge) * 4;
                        voxels[offset] = static_cast<std::uint8_t>(paletteIndex / kPaletteVariants);
                        voxels[offset + 1] = 255;
                        voxels[offset + 2] = static_cast<std::uint8_t>(paletteIndex % kPaletteVariants);
                        voxels[offset + 3] = 255;
                })) {
                std::cerr << "Could not convert region " << regionX << ", " << regionZ << ".\n";
                library.closeContext(context);
                return EXIT_FAILURE;
            }
            output.write(reinterpret_cast<const char*>(voxels.data()), static_cast<std::streamsize>(voxels.size()));
        }
    }

    library.closeContext(context);
    if (!output) {
        std::cerr << "Could not finish writing " << options.outputPath << ".\n";
        return EXIT_FAILURE;
    }

    const auto palettePath = options.palettePath.empty()
        ? (std::filesystem::path(options.outputPath).replace_extension(".vp").string())
        : options.palettePath;
    PaletteWriter palette;
    for (std::int32_t index = 0; index < static_cast<std::int32_t>(registry.patterns().size()); ++index)
        palette.addPattern(registry.patterns()[index], index);
    for (const auto material : { FallbackMaterial::grass, FallbackMaterial::dirt, FallbackMaterial::bedrock }) {
        if (registry.fallbackUsed(material))
            palette.addPattern(fullBlock(material, options.singleColor),
                PatternRegistry::kNormalCapacity + static_cast<std::int32_t>(material));
    }
    if (!palette.write(palettePath)) {
        std::cerr << "Could not write palette " << palettePath << ".\n";
        return EXIT_FAILURE;
    }

    std::cout << "Wrote " << chunkCount << " Voptix chunk(s) to " << options.outputPath
        << " and " << registry.patterns().size() << " palette variants to " << palettePath << ".\n";
    if (registry.overflowed()) {
        std::cerr << "Warning: palette capacity (" << PatternRegistry::kNormalCapacity
            << " terrain variants) was exceeded; " << registry.overflowedVariants()
            << " unique variants use a full-block fallback.\n";
    }
    return EXIT_SUCCESS;
}
