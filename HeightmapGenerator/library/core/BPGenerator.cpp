#include "Generators.h"
#include <filesystem>
#include <numbers>
#include <stdexcept>

namespace {
const Ort::Env& runtimeEnvironment() {
    static const Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "HeightmapGen");
    return env;
}

Ort::SessionOptions makeSessionOptions() {
    Ort::SessionOptions options;
    options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_DISABLE_ALL);
    return options;
}
}

/// @brief Fills out the given vector with numbers generated similarly to PyTorch.randn()
/// @param buffer Vector to be filled with random numbers
void torchRandn(std::vector<float>& buffer, std::seed_seq& seed) {
    std::mt19937 gen(seed);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    for (size_t i = 0; i < buffer.size(); i += 2)
    {
        float u1 = 1.0f - dist(gen); // avoid log(0)
        float u2 = 1.0f - dist(gen);
        float r = std::sqrt(-2.0f * std::log(u1));
        float theta = 2.0f * std::numbers::pi_v<float> * u2;
        
        buffer[i] = (r * std::cos(theta));
        if (i + 1 < buffer.size())
            buffer[i + 1] = (r * std::sin(theta));
    } 
} 

BPGeneratorImpl::BPGeneratorImpl(
        ContextImpl* ctx, 
        const CommonSettings& commonSettings, 
        const char* modelPath
)
:   GeneratorImpl(ctx, commonSettings),
    session(runtimeEnvironment(),
        std::filesystem::u8path(modelPath).c_str(),
        makeSessionOptions()
    ),
    memoryInfo(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault)),
    ioBinding(session),

    inputTensorElements(session.GetInputTypeInfo(0).GetTensorTypeAndShapeInfo().GetElementCount()),
    inputTensorShape(session.GetInputTypeInfo(0).GetTensorTypeAndShapeInfo().GetShape()),
    inputLayerName(session.GetInputNameAllocated(0, allocator)),
    inputBuffer(inputTensorElements),
    inputValue(Ort::Value::CreateTensor<float>(
        memoryInfo, 
        inputBuffer.data(), 
        inputBuffer.size(),
        inputTensorShape.data(),
        inputTensorShape.size()
    )),

    outputTensorElements(session.GetOutputTypeInfo(0).GetTensorTypeAndShapeInfo().GetElementCount()),
    outputTensorShape(session.GetOutputTypeInfo(0).GetTensorTypeAndShapeInfo().GetShape()),
    outputLayerName(session.GetOutputNameAllocated(0, allocator))    
{
    if (settings.resolution != outputTensorShape.back()) {
        throw std::runtime_error("Model output resolution does not match the requested resolution");
    }

    // Bind the reused input buffer
    ioBinding.BindInput(inputLayerName.get(), inputValue);
};

bool BPGeneratorImpl::isDeterministic() { return true; }

void BPGeneratorImpl::generateChunkData(Vec2Int chunkPos, float* buffer) {
    std::seed_seq seed{
        static_cast<uint32_t>(settings.seed),
        static_cast<uint32_t>(settings.seed >> 32),
        static_cast<uint32_t>(chunkPos.x),
        static_cast<uint32_t>(chunkPos.y)
    };
    torchRandn(inputBuffer, seed);

    // Create Ort::Value wrapping the user-supplied buffer
    auto outputValue = Ort::Value::CreateTensor<float>(
        memoryInfo,
        buffer,
        outputTensorElements,
        outputTensorShape.data(),
        outputTensorShape.size()      
    );

    // Make sure only the new value is bound to model output
    ioBinding.ClearBoundOutputs();
    ioBinding.BindOutput(outputLayerName.get(), outputValue);

    // Generate the chunk
    session.Run(runOptions, ioBinding);

    for (size_t i = 0; i < outputTensorElements; i++) {
		buffer[i] *= settings.amplitude;
	}
}

float BPGeneratorImpl::getHeight(Vec2Int pos) {
    int32_t stride = std::max(settings.resolution - 1, 1);
    Vec2Int chunkPos = pointToChunk(pos);
    // getChunk will create a new cache entry. Later getPoint calls will be able to reuse the cached data and getHeight will not be called again.
    std::vector<float> chunkData(settings.resolution * settings.resolution);
    getChunk(chunkPos, chunkData.data());
    Vec2Int localPos = pos - (chunkPos * stride);
    if (localPos.x < 0 || localPos.x >= settings.resolution || localPos.y < 0 || localPos.y >= settings.resolution) {
        return 0.0f;
    }
    float height = chunkData[localPos.x * settings.resolution + localPos.y];
    return height;
}
