# Terrain generation regression checks

Run from the repository root using the .NET 10:

```sh
dotnet run --project tests/UnityGeneration/Regression.csproj
dotnet run --project tests/UnityGeneration/NativeSmoke.csproj -- /absolute/path/to/HeightmapGen.dll
```

The first command tests the actual TerrainGen and TerrainGenerationSession sources with a simulated Unity host and native API. It covers captured settings, concurrent requests, synchronous/uncached fallback, bounded buffers, readiness ordering, repeated restart, slow cleanup, failure paths, disable/enable, teardown, Apply/R  input, and statistics. It exercises both coroutine hosts that dispose stopped enumerators and hosts that do not. This does not replace Unity Play Mode tests

The second command loads a built native library (DLL, .so, or .dylib). It compares the session's exact output to synchronous native calls for every GUI generator, caching mode, and zero/one/three workers, and repeats cancellation while work is outstanding.

The viewer exposes 0–64 workers; zero selects background synchronous generation. Caching disabled also uses that fallback. Requests and completed height buffers use a bounded window with a 64 MiB target for their combined float data, allowing at least one chunk. The native pool is capped by the pending window. Meshes, erosion scratch buffers, and a single oversized chunk are outside that target. Erosion samples its deterministic base without retaining a separate chunk cache.

Apply/R cancels and drains the old run on its background task before starting the latest captured settings. Meshes and statistics are applied on Unity's thread, one chunk per frame. Disabling cancels without blocking; destruction, Play Mode exit, and editor assembly reload wait for native cleanup. A native erosion job already running must finish before shutdown

The timing overlay reports elapsed wall time per displayed chunk, not the CPU duration of a chunk job. Brownian amplitude follows the corrected native implementation from main.
