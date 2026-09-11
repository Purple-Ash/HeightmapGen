#include "Generators.h"

/// @brief Fills out the given vector with numbers generated similarly to PyTorch.randn()
/// @param buffer Vector to be filled with random numbers
void torchRandn(std::vector<float> buffer) {
    thread_local std::random_device rnd;
    thread_local std::mt19937 gen(rnd());
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    for (size_t i = 0; i < buffer.size(); i += 2)
    {
        float u1 = 1.0f - dist(gen); // avoid log(0)
        float u2 = 1.0f - dist(gen);
        float r = std::sqrt(-2.0f * std::log(u1));
        float theta = 2.0f * static_cast<float>(M_PI) * u2;
        
        buffer[i] = (r * std::cos(theta));
        if (i + 1 < 1000)
            buffer[i + 1] = (r * std::sin(theta));
    } 
} 

BPGeneratorImpl::BPGeneratorImpl(
        ContextImpl* ctx, 
        const CommonSettings& commonSettings, 
        const Ort::Env& env,
        const Ort::SessionOptions& sessionOptions,
        const char* modelPath
)
:   GeneratorImpl(ctx, settings),
    session(env,
        modelPath,
        sessionOptions
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
    // Bind the reused input buffer
    ioBinding.BindInput(inputLayerName.get(), inputValue);

    // Override the resolution based on the mode output
    settings.resolution = outputTensorShape.back();
};

bool BPGeneratorImpl::isDeterministic() { return false; }
float BPGeneratorImpl::getHeight(float posX, float posY) { return 0.0f; }
int64_t BPGeneratorImpl::getResolution() { return settings.resolution; };
size_t BPGeneratorImpl::getOutputElementsCount() { return outputTensorElements; };

void BPGeneratorImpl::generateChunkData(int32_t chunkX, int32_t chunkY, float* buffer) {
    // Initialize the random input tensor
    torchRandn(inputBuffer);

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
}

