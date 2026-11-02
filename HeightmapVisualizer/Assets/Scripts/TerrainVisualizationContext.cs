using UnityEngine;

// One context per mesh. Presets may change colors, UV0, material and shader
// properties; geometry/normals remain the generator's responsibility.
public sealed class TerrainVisualizationContext
{
    public Mesh Mesh { get; }
    public Renderer Renderer { get; }
    public Transform Transform { get; }
    public float Amplitude { get; }
    public MaterialPropertyBlock Properties { get; } = new MaterialPropertyBlock();

    private readonly Color[] originalColors;
    private readonly Vector2[] originalUVs;
    private readonly Material[] originalMaterials;
    private readonly MaterialPropertyBlock originalProperties = new MaterialPropertyBlock();
    private GameObject decorations;

    // Future grass/detail instances can be parented here. They are removed
    // automatically when the preset changes or the chunk is destroyed.
    public Transform DecorationRoot
    {
        get
        {
            if (decorations == null)
            {
                decorations = new GameObject("Visualization Details");
                decorations.transform.SetParent(Transform, false);
            }
            return decorations.transform;
        }
    }

    public TerrainVisualizationContext(Mesh mesh, Renderer renderer, Transform transform, float amplitude)
    {
        Mesh = mesh;
        Renderer = renderer;
        Transform = transform;
        Amplitude = amplitude;
        originalColors = mesh.colors;
        originalUVs = mesh.uv;
        if (renderer != null)
        {
            originalMaterials = renderer.sharedMaterials;
            renderer.GetPropertyBlock(originalProperties);
            renderer.GetPropertyBlock(Properties);
        }
    }

    public void ApplyProperties()
    {
        if (Renderer != null) Renderer.SetPropertyBlock(Properties);
    }

    internal void Reset()
    {
        if (Mesh != null)
        {
            Mesh.colors = originalColors;
            Mesh.uv = originalUVs;
        }
        if (Renderer != null)
        {
            Renderer.sharedMaterials = originalMaterials;
            Renderer.SetPropertyBlock(originalProperties);
            Renderer.GetPropertyBlock(Properties);
        }
        if (decorations != null)
        {
            decorations.SetActive(false);
            Object.Destroy(decorations);
            decorations = null;
        }
    }
}
