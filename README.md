# HeightmapGen
Analysis of algorithms for generating realistic terrain height maps in real time

## Running BPGenerator
1. First you'll need the .onnx file with exported compatible model. Suggested directory is `./models/`, but any will work as long as you set the correct path.
2. Get the right ONNX Runtime distributable from the [docs](https://onnxruntime.ai/docs/install/) or [github](https://github.com/microsoft/onnxruntime/releases/)
    - Windows - put your files in `./third-party/onnxruntime`
    - Linux - put your files in `/opt/onnxruntime/`

> Running in dev-container: `.devcontainer/` includes a Dockerfile that will automatically set up a Debian environment with ONNX Runtime and C++ toolchain already installed. Requires dev containers capable IDE (or manual tinkering with Docker)