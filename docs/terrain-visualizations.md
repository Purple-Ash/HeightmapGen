# Terrain visualization presets

## Editor setup

1. On the **Generator** object that has `TerrainGen`, add **Terrain > Visualization Menu**. Unity also adds the required `TerrainVisualizationController`.
2. Create two empty GameObjects and add **Terrain > Height Gradient Visualization** to each. They can remain scene objects or be saved as prefab assets.
3. Give them display names such as **Green–Red** and **Black–White**. In each component's context menu, choose **Use Green-Red Gradient** or **Use Black-White Gradient**. You can also edit the Gradient directly.
4. Import each 100 × 150 PNG as **Sprite (2D and UI)** and assign it to the preset's **Icon** field. Without an icon, a plain card with the display name is shown.
5. On the Generator's **Visualization Controller**, add both preset components to **Visualizations** in the desired left-to-right order. Drag a scene GameObject or prefab containing the component into each entry. **Default Visualization** is the zero-based index initially selected.
6. Enter Play Mode. Press **Esc** to release the camera cursor, then click a card in the bottom-left panel. Cards are horizontal; drag or scroll when the row is wider than the screen. Pressing **Esc** again while the cursor is released opens **Do you want to quit?**. **No** or another **Esc** closes the popup and keeps the cursor released; **Yes** quits the desktop build or stops Play Mode in the editor. The popup blocks clicks on underlying controls, camera movement, regeneration hotkeys, and visualization number shortcuts while open.

The menu appears when at least one preset is assigned. It uses the scene's existing EventSystem, like the current settings panel. Existing chunks change immediately without regenerating; later chunks use the current selection. Selection survives Apply/R regeneration.

The icon itself is 100 × 150 UI pixels, with a border and name below it. The panel has its own screen-space canvas and fits the screen width.

## Height settings

Both color schemes use the same script. With **Relative To Amplitude** enabled, the height is divided by the amplitude captured when that chunk was generated. A **Height Range** of `(0, 2)` reproduces the original green-to-red mapping within that range. Values outside the range clamp to the endpoint colors.

Keep the same range on both presets for a direct comparison. If the terrain never reaches twice its amplitude, the highest endpoint color will not appear; use `(0, 1)` or another range if appropriate for the generated heights. With Relative To Amplitude disabled, the range uses mesh height units instead. Zero amplitude is handled without division by zero.

Leave **Surface Material** empty to keep the chunk's existing `GroundVisualisation` material. A replacement material must support vertex colors to display this gradient. The existing shader applies texture and lighting to those colors, so the result is shaded rather than a raw grayscale heightmap.

## Adding textures, grass, or another visualization

Derive a new component from `TerrainVisualization`. It implements `ITerrainVisualization`, which supplies `DisplayName`, `Icon`, `Apply(context)`, and `Release(context)`.

- `context.Mesh` provides vertices, normals, UVs, and vertex colors. Heights are `vertices[i].y`; world positions can be obtained with `context.Transform.TransformPoint(...)`.
- Call the inherited `ApplyMaterial(context)` to use the optional preset material. Assign texture/shader values through `context.Properties`, then call `context.ApplyProperties()`. This avoids modifying a shared material asset.
- Parent generated grass/detail objects to `context.DecorationRoot`. These are removed automatically on selection changes and chunk cleanup.
- Override `Release(context)` to free any other resources the preset creates, such as buffers or runtime materials.

Presets are shared configuration components. Their methods can be called directly on prefab references, so don't depend on `Awake`, `Start`, `Update`, or a preset's scene position. Use the supplied chunk context for runtime work and keep any extra per-chunk state separate for each context.

On switching, the chunk restores its original vertex colors, UV0, material assignments, and renderer-wide property block, then applies the new preset. Geometry, normals, and colliders belong to generation and should not be modified by a visualization. Per-material property blocks and additional mesh channels are not managed automatically; a custom preset that changes them must restore them in `Release`.

With no controller or no presets, the original Chunk coloring continues to work. No scenes, prefab assets, materials, or icons are created or configured by this change.

## Gradients by slope

Add **Terrain > Slope Gradient Visualization** to another preset object or prefab, then assign its icon/name and add it to the controller's Visualizations list as usual.

Each **Slope Gradients** entry has a Minimum Slope, Maximum Slope (degrees), and its own height Gradient. The shared Relative To Amplitude and Height Range fields work like the height-only preset. Both neighboring gradients are evaluated at the same height before their resulting colors are blended.

For ranges `0-15` and `25-40`:

- `0-15` degrees uses the first gradient entirely.
- `15-25` degrees transitions between gradients: at 20 degrees, the blend position is halfway. The color path depends on Blend Mode.
- `25-40` degrees uses the second gradient entirely.
- Above 40 degrees, the second gradient continues to apply. Below the first range, the first gradient applies.

Add as many ranges as needed. The script sorts a copy of them at application time, so Inspector ordering does not matter. Ranges must stay within 0-90 degrees, have minimum <= maximum, and not overlap; invalid configuration logs an error and the chunk restores its original appearance. Touching ranges have no blend interval, with the lower range owning the shared boundary. Use a positive gap for a smooth transition.

Slope is the angle between the rendered vertex normal and world up, including the chunk transform. The normals are averaged across the triangles sharing each vertex within that chunk; these are not the per-triangle slope statistics. Heights remain local mesh heights, consistent with the height-only preset. Configure before entering Play Mode, or switch away and back to reapply edited gradients during Play Mode.

### Richer slope transitions

**Blend Mode** defaults to **Rich Hsv**: it interpolates hue along the shorter path and handles saturation and brightness separately. This avoids the dull RGB midpoint when contrasting hues are mixed. **Blend Richness** (default 0.35) retains more of the stronger endpoint saturation in the middle of the transition. Increase it toward 1 for richer transitions; 0 uses ordinary HSV interpolation. **Rgb** restores the original mixing for comparison.

The richness adjustment fades to zero at both ends of each gap. Colors within the solid slope ranges, the configured gradients, and transparency at the endpoints remain unchanged. Greys remain neutral when blended with other greys, and fades to black keep the colored endpoint's hue. This changes only slope-gap blending, not interpolation inside each height gradient or material lighting. While playing, switch to another preset and back to see changed blend settings.

### Material saturation, contrast, and brightness

Select **GroundVisualisation** (or another material using **Custom/VertexColorVF**) in the Project window. Under **Color Adjustments**, all three sliders use `1` as neutral:

- **Saturation:** `0` is grayscale, below `1` is muted, and above `1` strengthens color.
- **Contrast:** below `1` softens tonal differences; above `1` increases them around a midpoint of `0.5` in the shader's working color space.
- **Brightness:** a multiplier applied last. Try `0.8` to darken the result; `0` is black and `2` doubles the output.

These controls operate after the existing texture, vertex color, and lighting calculation, update directly on the rendered material, and do not change alpha. Neutral values preserve the existing RGB appearance. The existing lighting formula can boost highlights; brightness can reduce that final output.

All current presets inherit the chunk's GroundVisualisation material unless a Surface Material override is assigned. For independent adjustments, duplicate GroundVisualisation, tune the duplicate, and assign it to the desired preset's **Surface Material**. Materials using other shaders (such as the built-in shader currently assigned to SolidColor) do not gain these controls automatically.


## Icons and keyboard selection

The five PNGs currently in **Assets/Images** are imported as single sprites, ready to drag into each visualization preset's **Icon** field. Their asset GUIDs and PNG contents are preserved. For future images, select them in the Project window, set **Texture Type > Sprite (2D and UI)** and **Sprite Mode > Single**, then click **Apply**. You can select several images to change these settings together. See [Unity's sprite import settings](https://docs.unity3d.com/2023.2/Documentation/Manual/texture-type-sprite.html).

During Play Mode, use **1-9** to select the first nine visible cards and **0** for the tenth. The numpad works too. Card labels display their shortcut. The order follows the controller's Visualizations list, skipping empty entries just like the menu. Cards beyond the tenth remain clickable. Number keys work with the camera cursor locked or unlocked, and update the selected border and generated chunks through the same selection path as clicking a card.

Shortcuts are ignored while a settings InputField has focus, so typing seed, resolution, or other numbers will not switch presets. Click outside the field to use shortcuts again. Ctrl, Alt, and Command/Windows combinations are ignored. In the editor, focus the Game view to send it keyboard input.
