# Terrain atmosphere

Assign **Assets/Materials/TerrainAtmosphere.mat** to a visualization preset's **Surface Material** field. It uses **Custom/TerrainAtmosphere**, an opaque URP shader that supports the existing height and slope vertex colors, texture, and saturation/contrast/brightness controls. Alternatively, assign it to the Chunk prefab's renderer to make it the default for presets with no material override. During Play Mode, switch away and back after changing a preset's material reference. Changes to the material's sliders update directly.

Select the **material asset** in the Project window to see these controls:

| Control | Effect |
| --- | --- |
| Haze Color | Blue tint approached by distant terrain. |
| Start Distance | Distance from the camera before blue haze begins, in world units. |
| Haze Density | How quickly haze builds beyond that distance. |
| Maximum Haze Opacity | Limits how much the haze hides terrain; set to 0 to disable it. |
| Ground Fog Color | Dark blue-grey by default, subtly darkening and cooling low terrain. |
| Full Density Height | World Y below which the ground layer has full density. |
| Fade Height Above Base | Thickness over which density fades upward to zero. |
| Ground Fog Density | How quickly fog accumulates along the portion of the view ray inside the layer. |
| Maximum Ground Fog Opacity | Caps the ground fog effect; default 0.25, or 0 to disable it. |

Defaults are a starting point for the current scene's terrain amplitude of 45: blue haze begins at 100 units with density 0.002; the ground layer is densest below Y=15 and disappears by Y=50. Adjust these world-space values to match terrain size and altitude. Increasing density brings the effect closer. A larger fade height softens the vertical transition. A lighter Ground Fog Color produces mist instead of a dim blue-grey layer. Fog blends toward its color, so very dark surfaces can become brighter even with a dark fog color.

Both layers use exponential attenuation with a maximum opacity. Ground density is integrated analytically along the camera-to-surface segment, which gives long low views more fog than short views and avoids stepped sampling bands. It is a horizontal valley layer in world space; it does not follow every hill. With both opacity sliders at 0, only the directionally lit terrain and its color grading remain. Grading happens before fog so atmospheric colors remain independently adjustable.

This is a terrain surface effect, not a fullscreen or volumetric renderer. It affects meshes using this material, not the sky, other materials, or empty air, and does not add volumetric light shafts. The terrain itself casts and receives realtime shadows. No camera depth texture, renderer feature, or Unity global fog setting is required. Fog color alpha is unused; use the opacity sliders. For a seamless horizon, tune Haze Color toward the sky's horizon color and increase Maximum Haze Opacity.

The shader uses URP's world-space transformation and camera helpers from [Unity's custom shader documentation](https://docs.unity.com/en-us/engine/6000.0/manual/materials-and-shaders/shaders/writing-custom-shaders-urp/use-built-in-shader-methods/camera).


## Directional lighting and shadows

TerrainAtmosphere uses the scene's main Directional Light for diffuse lighting, including its rotation, color, intensity, and realtime shadows. The environment's ambient lighting fills unlit slopes. Color grading is applied after lighting and before the atmosphere. This replaces the previous fixed light direction; existing material values are preserved, but the rendered brightness will change with the scene lighting.

SampleScene's existing Directional Light is explicitly assigned as its Sun Source. The light and saved Scene-view lighting toggle were already enabled. Chunk renderers already cast and receive shadows. The PC pipeline's shadow distance is now 300 world units (previously 50); it retains the existing four cascades and soft shadows. This extends visible terrain shadows at some rendering cost. Adjust **PC_RPAsset > Shadows > Max Distance** to trade coverage against sharpness and performance. The Mobile pipeline keeps its existing distance.

The shader includes URP ShadowCaster, DepthOnly, and DepthNormalsOnly passes, and supports the scene's screen-space ambient occlusion. The material is opaque; texture/color alpha does not cut holes in shadows. Directional lighting here is diffuse plus ambient, without specular highlights or additional point/spot light illumination. Ground fog and blue haze still blend over the lit result.

To check it, select a preset using TerrainAtmosphere, rotate the Directional Light, and view a ridge within the shadow distance. Light-facing slopes should brighten and ridges should cast shadows. If only the Scene view looks unlit, enable its **Scene Lighting** (sun) toggle; the Game view uses the scene's lights independently. See [Unity's URP shadow documentation](https://docs.unity3d.com/6000.0/Documentation/Manual/urp/use-built-in-shader-methods-shadows.html) for the underlying shadow APIs.
