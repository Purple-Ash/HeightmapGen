using UnityEngine;

[AddComponentMenu("Terrain/Height Gradient Visualization")]
public sealed class HeightGradientVisualization : TerrainVisualization
{
    [SerializeField] private Gradient gradient = CreateGradient(Color.green, Color.red);
    [Tooltip("Divide vertex heights by the amplitude captured when this chunk was generated.")]
    [SerializeField] private bool relativeToAmplitude = true;
    [Tooltip("Heights at the first and last gradient colors. 0 to 2 matches the original green-red mapping when relative to amplitude.")]
    [SerializeField] private Vector2 heightRange = new Vector2(0f, 2f);

    public override void Apply(TerrainVisualizationContext context)
    {
        ApplyMaterial(context);
        Vector3[] vertices = context.Mesh.vertices;
        Color[] colors = new Color[vertices.Length];
        for (int i = 0; i < vertices.Length; i++)
        {
            float height = vertices[i].y;
            if (relativeToAmplitude)
            {
                height = Mathf.Abs(context.Amplitude) > Mathf.Epsilon ? height / context.Amplitude : 0f;
            }
            colors[i] = gradient.Evaluate(Mathf.InverseLerp(heightRange.x, heightRange.y, height));
        }
        context.Mesh.colors = colors;
    }

    [ContextMenu("Use Green-Red Gradient")]
    private void UseGreenRedGradient()
    {
        SetGradient(Color.green, Color.red);
    }

    [ContextMenu("Use Black-White Gradient")]
    private void UseBlackWhiteGradient()
    {
        SetGradient(Color.black, Color.white);
    }

    private void SetGradient(Color low, Color high)
    {
#if UNITY_EDITOR
        UnityEditor.Undo.RecordObject(this, "Change height gradient");
#endif
        gradient = CreateGradient(low, high);
#if UNITY_EDITOR
        UnityEditor.EditorUtility.SetDirty(this);
        if (UnityEditor.PrefabUtility.IsPartOfPrefabInstance(this))
        {
            UnityEditor.PrefabUtility.RecordPrefabInstancePropertyModifications(this);
        }
#endif
    }

    private static Gradient CreateGradient(Color low, Color high)
    {
        Gradient result = new Gradient();
        result.SetKeys(
            new[] { new GradientColorKey(low, 0f), new GradientColorKey(high, 1f) },
            new[] { new GradientAlphaKey(1f, 0f), new GradientAlphaKey(1f, 1f) });
        return result;
    }
}
