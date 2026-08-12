# Voptix data generator

`VoptixDataGenerator` dynamically links to `HeightmapGen` and writes a Voptix
`.vx` world file and a matching `.vp` palette. It converts each generated
heightmap region into one Voptix 128 x 128 x 128 voxel chunk. Terrain is
represented by deduplicated palette variants, with grass, dirt, and bedrock
colours encoded directly into the generated palette.

## Build

From the repository root:

```powershell
cmake -S VoptixDataGenerator -B build/voptix-data-generator
cmake --build build/voptix-data-generator --config Release
```

The converter does not build or link against `HeightmapGen`. It loads the DLL
at runtime, looking beside the executable unless `--dll` supplies another path.
The header used to build the converter must match that DLL's public API.

```powershell
cmake -S VoptixDataGenerator -B build/voptix-data-generator `
  -DVOPTIX_HEIGHTMAPGEN_DLL=C:/path/to/HeightmapGen.dll
```

## Generate and load

```powershell
./build/voptix-data-generator/Release/VoptixDataGenerator.exe terrain.vx
```

Use `--dll C:/path/to/HeightmapGen.dll` to override the DLL at runtime. Without
that option, the executable loads `HeightmapGen.dll` from its own directory.
The default generator is Brownian Perlin. Use `--generator random`,
`--generator perlin`, or `--generator brownian` to select the original modes.

## Hydraulic erosion

Use the CPU hydraulic erosion generator from `HeightmapGen` with an updated DLL:

```powershell
./build/voptix-data-generator/Release/VoptixDataGenerator.exe eroded.vx `
  --dll C:/path/to/HeightmapGen.dll `
  --generator hydraulic `
  --erosion-seed 42 `
  --erosion-iterations 10
```

Hydraulic mode requests one native 128 x 128 heightmap region for each Voptix
chunk. This lets droplets erode the entire visible chunk instead of independently
eroding the smaller regions normally assembled by the converter. HeightmapGen's
padded-border behavior reduces edge artifacts, but separately generated Voptix
chunks are not guaranteed to have identical eroded boundary samples.

The remaining optional controls mirror `HydraulicErosionSettings`:
`--erosion-radius`, `--erosion-lifetime`, `--erosion-inertia`,
`--erosion-capacity`, `--erosion-min-capacity`, `--erosion-erode-speed`,
`--erosion-deposit-speed`, `--erosion-evaporation`, `--erosion-gravity`,
`--erosion-start-speed`, and `--erosion-start-water`. Run the executable without
arguments to see their defaults and accepted ranges.

The default produces a 3 x 3 world at Voptix chunk positions `(19..21, 0,
19..21)`, which is centered at Voptix's loading position. It also creates
`terrain.vp` in the same directory. Select both files with Voptix's **Open**
button. The file format is specific to the current Voptix source: it assumes
an octree depth of 7 (128 voxels per chunk) and an 8 x 8 x 8 palette block.

Use `--subdivision 1`, `2`, `4`, or `8` to choose how many independently
sampled cells lie along a palette block edge. The fixed 8 x 8 x 8 Voptix block
is filled in groups, so `2` creates 4 x 4 x 4 subvoxel groups and `4` creates
2 x 2 x 2 groups. The default of `2` provides smooth slopes without exhausting
the Voptix capacity of 2,048 palette variants for typical worlds. The palette
stores only variants used by the terrain; unused Voptix entries are transparent.
If more terrain patterns are needed than fit, the exporter prints a warning and
uses a matching full grass, dirt, or bedrock block for each excess pattern.

Add `--single-color` to render every terrain block in one olive-green colour.
It removes the grass/dirt/bedrock colour distinctions from the variant key, so
more slope and edge shapes fit in the same palette. For example:

```powershell
./build/voptix-data-generator/Release/VoptixDataGenerator.exe terrain.vx --subdivision 4 --single-color
```

Voptix begins with its camera at height 65 and cannot render correctly when
the camera starts inside a solid voxel. The default amplitude is therefore 32
(Brownian Perlin terrain remains below the camera). If you increase
`--amplitude`, keep it at 34 or below initially, then move the camera upward
before loading taller worlds.

Run the executable without arguments to show all options, including region
count, terrain amplitude, noise scale, and Perlin generator selection.
