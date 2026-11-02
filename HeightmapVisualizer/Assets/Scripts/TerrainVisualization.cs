using UnityEngine;

public interface ITerrainVisualization
{
    string DisplayName { get; }
    Sprite Icon { get; }
    void Apply(TerrainVisualizationContext context);
    void Release(TerrainVisualizationContext context);
}

// Presets are configuration components, referenced from the scene or a prefab.
// Keep per-chunk resources in the context, not in the shared preset component.
public abstract class TerrainVisualization : MonoBehaviour, ITerrainVisualization
{
    [SerializeField] private string displayName;
    [SerializeField] private Sprite icon;
    [Tooltip("Optional material override. Leave empty to use the chunk's material.")]
    [SerializeField] private Material surfaceMaterial;

    public string DisplayName => string.IsNullOrWhiteSpace(displayName) ? gameObject.name : displayName;
    public Sprite Icon => icon;

    public abstract void Apply(TerrainVisualizationContext context);

    // Called on switching presets, rebuilding a mesh, or destroying a chunk.
    public virtual void Release(TerrainVisualizationContext context) { }

    protected void ApplyMaterial(TerrainVisualizationContext context)
    {
        if (surfaceMaterial != null && context.Renderer != null)
        {
            context.Renderer.sharedMaterial = surfaceMaterial;
        }
    }
}
