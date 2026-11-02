# Topographic material

**Assets/Materials/Topographic.mat** uses **Custom/Topographic**. It keeps GroundVisualisation's vertex gradient colors, texture, fixed shading, and saturation/contrast/brightness controls, then overlays horizontal contours at equal world elevations. Your existing **Topographic** prefab (display name **Classic Topographic**) uses this material. Other presets can use it through their **Surface Material** field.

Select the material asset to find **Topographic Lines**:

| Setting | Effect |
| --- | --- |
| Line Density | Contours per world-height unit. `0.2` means one contour every 5 units; `0.1` every 10 units; `1` every unit. Zero disables contours. |
| Line Width | Approximate thickness in render pixels, independent of elevation spacing. Default `1.5`; zero disables contours. |
| Line Color | Contour color; its alpha controls opacity. |
| Height Offset | World elevation of a reference contour. Other contours repeat above and below it at the chosen spacing. |

The contours follow world Y, so matching elevations meet across chunks, even when a chunk has its own transform. They are contour curves on the terrain, not mesh wireframe edges. A perfectly flat patch has no elevation boundary to trace and is left unlined. Density does not scale automatically with generation amplitude: increase density for smaller terrains and decrease it for larger height ranges.

Pixel derivatives soften edges, and contours fade when their spacing becomes too small to resolve clearly. Width is approximate and may vary slightly with screen orientation and render scale. The underlying [HLSL fwidth function](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-fwidth) measures the height variation across neighboring pixels. Extremely dense lines or distant terrain may therefore look unlined; reduce density to separate them.

Like GroundVisualisation, this material uses fixed stylized shading. Directional lighting, realtime shadows, and atmospheric fog belong to TerrainAtmosphere and are not part of this shader. Material sliders update immediately; if changing a preset's Surface Material during Play Mode, switch away and back to reapply it.
