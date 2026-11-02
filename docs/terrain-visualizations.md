# Terrain visualization presets

## Editor setup

1. On the **Generator** object that has `TerrainGen`, add **Terrain > Visualization Menu**. Unity also adds the required `TerrainVisualizationController`.
2. Create two empty GameObjects and add **Terrain > Height Gradient Visualization** to each. They can remain scene objects or be saved as prefab assets.
3. Give them display names such as **Green–Red** and **Black–White**. In each component's context menu, choose **Use Green-Red Gradient** or **Use Black-White Gradient**. You can also edit the Gradient directly.
4. Import each 100 × 150 PNG as **Sprite (2D and UI)** and assign it to the preset's **Icon** field. Without an icon, a plain card with the display name is shown.
5. On the Generator's **Visualization Controller**, add both preset components to **Visualizations** in the desired left-to-right order. Drag a scene GameObject or prefab containing the component into each entry. **Default Visualization** is the zero-based index initially selected.
6. Enter Play Mode. Press **Esc** to release the camera cursor, then click a card in the bottom-left panel. Cards are horizontal; drag or scroll when the row is wider than the screen.

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
