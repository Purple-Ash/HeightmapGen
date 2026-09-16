# DatasetGenerator

`DatasetGenerator` creates paired heightmap datasets for the HeightmapGen library:

- `in/`: Brownian-Perlin input heightmaps
- `out/`: hydraulically eroded output heightmaps

Each sample is a square raw `float32` file named `000000.bin`, `000001.bin`, and so on.

## Build

Build the shared library first, then build this tool from the repository root:

```sh
cmake -S HeightmapGenerator -B HeightmapGenerator/build
cmake --build HeightmapGenerator/build
cmake -S DatasetGenerator -B DatasetGenerator/build
cmake --build DatasetGenerator/build
```

## Generate a dataset

From the repository root on Linux:

```sh
DatasetGenerator/build/DatasetGenerator --output DatasetGenerator/dataset --dll HeightmapGenerator/build/libHeightmapGen.so
```

On Windows, pass the path to `HeightmapGen.dll` with `--dll`.

## View samples

Install the viewer dependencies:

```sh
python3 -m pip install matplotlib numpy
```

View a single sample (e.g. 0), or a range of samples (e.g. 2-4):

```sh
python3 DatasetGenerator/src/viewer.py DatasetGenerator/dataset 0
python3 DatasetGenerator/src/viewer.py DatasetGenerator/dataset 2-4
```
