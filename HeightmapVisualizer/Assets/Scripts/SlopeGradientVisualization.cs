using System;
using System.Collections.Generic;
using UnityEngine;

[AddComponentMenu("Terrain/Slope Gradient Visualization")]
public sealed class SlopeGradientVisualization : TerrainVisualization
{
    public enum SlopeBlendMode
    {
        RichHsv,
        Rgb
    }

    [Serializable]
    public sealed class SlopeGradient
    {
        [Range(0f, 90f)] public float minimumSlope;
        [Range(0f, 90f)] public float maximumSlope = 15f;
        public Gradient gradient = new Gradient();
    }

    [Tooltip("Non-overlapping slope ranges in degrees. Each gradient is fully used inside its range; gaps blend the neighboring gradients. Order does not matter.")]
    [SerializeField] private List<SlopeGradient> slopeGradients = new List<SlopeGradient>
    {
        new SlopeGradient { minimumSlope = 0f, maximumSlope = 15f, gradient = CreateGradient(Color.green, Color.red) },
        new SlopeGradient { minimumSlope = 25f, maximumSlope = 40f, gradient = CreateGradient(Color.black, Color.white) }
    };
    [Tooltip("Divide vertex heights by the amplitude captured when this chunk was generated.")]
    [SerializeField] private bool relativeToAmplitude = true;
    [Tooltip("Heights at the first and last color of every gradient, shared across all slope ranges.")]
    [SerializeField] private Vector2 heightRange = new Vector2(0f, 2f);

    [Header("Slope Transitions")]
    [Tooltip("Rich HSV preserves color saturation between slope ranges. RGB uses the original channel-by-channel blend.")]
    [SerializeField] private SlopeBlendMode blendMode = SlopeBlendMode.RichHsv;
    [Tooltip("In Rich HSV mode, retain more of the stronger saturation through the middle of a transition. Endpoint colors stay unchanged.")]
    [Range(0f, 1f)][SerializeField] private float blendRichness = 0.35f;

    public override void Apply(TerrainVisualizationContext context)
    {
        SlopeGradient[] bands = GetSortedGradients();
        Vector3[] vertices = context.Mesh.vertices;
        Vector3[] normals = context.Mesh.normals;
        if (normals.Length != vertices.Length)
        {
            throw new InvalidOperationException("Slope visualization requires a normal for every mesh vertex.");
        }

        // Transform normals correctly even when the terrain has nonuniform scale.
        Matrix4x4 normalMatrix = context.Transform.localToWorldMatrix.inverse.transpose;
        Color[] colors = new Color[vertices.Length];
        for (int i = 0; i < vertices.Length; i++)
        {
            float height = vertices[i].y;
            if (relativeToAmplitude)
            {
                height = Mathf.Abs(context.Amplitude) > Mathf.Epsilon ? height / context.Amplitude : 0f;
            }
            float heightPosition = Mathf.InverseLerp(heightRange.x, heightRange.y, height);
            Vector3 worldNormal = normalMatrix.MultiplyVector(normals[i]);
            float slope = Mathf.Clamp(Vector3.Angle(Vector3.up, worldNormal), 0f, 90f);
            colors[i] = EvaluateColor(bands, slope, heightPosition);
        }

        ApplyMaterial(context);
        context.Mesh.colors = colors;
    }

    private SlopeGradient[] GetSortedGradients()
    {
        if (slopeGradients == null || slopeGradients.Count == 0)
        {
            throw new InvalidOperationException("Add at least one slope gradient to this visualization preset.");
        }

        SlopeGradient[] bands = slopeGradients.ToArray();
        foreach (SlopeGradient band in bands)
        {
            if (band == null || band.gradient == null ||
                !(band.minimumSlope >= 0f && band.maximumSlope <= 90f && band.minimumSlope <= band.maximumSlope))
            {
                throw new InvalidOperationException("Each slope gradient needs a valid gradient and a minimum <= maximum between 0 and 90 degrees.");
            }
        }
        Array.Sort(bands, (a, b) => a.minimumSlope.CompareTo(b.minimumSlope));
        for (int i = 1; i < bands.Length; i++)
        {
            if (bands[i].minimumSlope < bands[i - 1].maximumSlope)
            {
                throw new InvalidOperationException("Slope gradient ranges must not overlap. Leave a gap between ranges to blend them.");
            }
        }
        return bands;
    }

    private Color EvaluateColor(SlopeGradient[] bands, float slope, float heightPosition)
    {
        // Below the first range and above the last, keep the nearest gradient.
        if (slope <= bands[0].maximumSlope) return bands[0].gradient.Evaluate(heightPosition);

        for (int i = 1; i < bands.Length; i++)
        {
            SlopeGradient previous = bands[i - 1];
            SlopeGradient current = bands[i];
            if (slope < current.minimumSlope)
            {
                float blend = Mathf.InverseLerp(previous.maximumSlope, current.minimumSlope, slope);
                Color from = previous.gradient.Evaluate(heightPosition);
                Color to = current.gradient.Evaluate(heightPosition);
                return blendMode == SlopeBlendMode.Rgb
                    ? Color.Lerp(from, to, blend)
                    : BlendRichColors(from, to, blend, blendRichness);
            }
            if (slope <= current.maximumSlope) return current.gradient.Evaluate(heightPosition);
        }
        return bands[bands.Length - 1].gradient.Evaluate(heightPosition);
    }

    private static Color BlendRichColors(Color from, Color to, float blend, float richness)
    {
        if (blend <= 0f) return from;
        if (blend >= 1f) return to;

        Color.RGBToHSV(from, out float fromHue, out float fromSaturation, out float fromValue);
        Color.RGBToHSV(to, out float toHue, out float toSaturation, out float toValue);

        // Grey has no meaningful hue. Black also has no meaningful saturation:
        // borrow the colored endpoint's values instead of taking a detour via red.
        const float achromaticThreshold = 0.000001f;
        if (fromValue <= achromaticThreshold)
        {
            fromHue = toHue;
            fromSaturation = toSaturation;
        }
        else if (fromSaturation <= achromaticThreshold) fromHue = toHue;

        if (toValue <= achromaticThreshold)
        {
            toHue = fromHue;
            toSaturation = fromSaturation;
        }
        else if (toSaturation <= achromaticThreshold) toHue = fromHue;

        float hue = Mathf.Repeat(Mathf.LerpAngle(fromHue * 360f, toHue * 360f, blend), 360f) / 360f;
        float saturation = Mathf.Lerp(fromSaturation, toSaturation, blend);
        float middleWeight = 4f * blend * (1f - blend);
        saturation = Mathf.Lerp(saturation, Mathf.Max(fromSaturation, toSaturation),
            Mathf.Clamp01(richness) * middleWeight);
        float value = Mathf.Lerp(fromValue, toValue, blend);
        Color result = Color.HSVToRGB(hue, saturation, value, true);
        result.a = Mathf.Lerp(from.a, to.a, blend);
        return result;
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
