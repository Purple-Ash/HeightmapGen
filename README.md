# HeightmapGen
Analysis of algorithms for generating realistic terrain height maps in real time

## Unity BP visualizer

In the `TerrainGen` component, select `BPGenerator` as the generator type.
Place your ONNX model in `HeightmapVisualizer/Assets/StreamingAssets` and set 
**BP ModelPath** to its filename. Set **Resolution** to match the model's output resolution.

The Unity `Assets/Plugins` folder needs a current `HeightmapGen.dll`, along with its ONNX Runtime DLL dependencies.

## Dev-container

Running in dev-container: `.devcontainer/` includes a Dockerfile that will automatically set up a Debian environment with C++ toolchain already installed. Requires dev containers capable IDE (or manual tinkering with Docker)
