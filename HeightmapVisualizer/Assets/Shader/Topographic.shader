Shader "Custom/Topographic"
{
    Properties
    {
        _Color ("Color", Color) = (1,1,1,1)
        _MainTex ("Texture", 2D) = "white" {}

        [Header(Color Adjustments)]
        _Saturation ("Saturation", Range(0,2)) = 1
        _Contrast ("Contrast", Range(0,2)) = 1
        _Brightness ("Brightness", Range(0,2)) = 1

        [Header(Topographic Lines)]
        _LineColor ("Line Color (Alpha = Opacity)", Color) = (0.06,0.07,0.05,0.85)
        _LineDensity ("Line Density (Lines Per Height Unit)", Float) = 0.2
        _LineWidth ("Line Width (Pixels)", Range(0,8)) = 1.5
        _LineOffset ("Height Offset (World Units)", Float) = 0
    }

    SubShader
    {
        Tags { "RenderType"="Opaque" "Queue"="Geometry" "RenderPipeline"="UniversalPipeline" }

        Pass
        {
            Name "Topographic"
            Tags { "LightMode"="UniversalForwardOnly" }
            ZWrite On
            Cull Back

            HLSLPROGRAM
            #pragma vertex vert
            #pragma fragment frag
            #pragma target 3.0

            #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Core.hlsl"

            TEXTURE2D(_MainTex);
            SAMPLER(sampler_MainTex);

            CBUFFER_START(UnityPerMaterial)
                float4 _MainTex_ST;
                float4 _Color;
                float4 _LineColor;
                float _Saturation;
                float _Contrast;
                float _Brightness;
                float _LineDensity;
                float _LineWidth;
                float _LineOffset;
            CBUFFER_END

            struct Attributes
            {
                float4 positionOS : POSITION;
                float3 normalOS : NORMAL;
                float2 uv : TEXCOORD0;
                float4 color : COLOR;
            };

            struct Varyings
            {
                float4 positionCS : SV_POSITION;
                float2 uv : TEXCOORD0;
                float3 normalWS : TEXCOORD1;
                float heightWS : TEXCOORD2;
                float4 color : COLOR;
            };

            Varyings vert(Attributes input)
            {
                Varyings output;
                output.positionCS = TransformObjectToHClip(input.positionOS.xyz);
                output.normalWS = TransformObjectToWorldNormal(input.normalOS);
                output.heightWS = TransformObjectToWorld(input.positionOS.xyz).y;
                output.uv = TRANSFORM_TEX(input.uv, _MainTex);
                output.color = input.color;
                return output;
            }

            float3 AdjustColor(float3 color)
            {
                float luminance = dot(color, float3(0.2126, 0.7152, 0.0722));
                color = lerp(luminance.xxx, color, _Saturation);
                color = (color - 0.5) * _Contrast + 0.5;
                return max(color, 0.0) * _Brightness;
            }

            float ContourMask(float worldHeight)
            {
                float coordinate = (worldHeight - _LineOffset) * max(_LineDensity, 0.0);
                // Differentiate the continuous height before wrapping to avoid seams at each line.
                float footprint = fwidth(coordinate);
                float width = max(_LineWidth, 0.0);
                if (_LineDensity <= 0.0 || width <= 0.0 || footprint <= 0.000001)
                    return 0.0;

                // Nearest integer elevation, including below world Y=0.
                float distanceToLine = abs(frac(coordinate + 0.5) - 0.5);
                float distanceInPixels = distanceToLine / footprint;
                float halfWidth = width * 0.5;
                float coverage = 1.0 - smoothstep(max(halfWidth - 0.5, 0.0), halfWidth + 0.5, distanceInPixels);

                // Fade subpixel lines and crowded contours instead of letting them shimmer or fill the terrain.
                float spacingInPixels = 1.0 / footprint;
                float visibility = smoothstep(width + 1.0, width + 3.0, spacingInPixels);
                return coverage * min(width, 1.0) * visibility;
            }

            float4 frag(Varyings input) : SV_Target
            {
                float4 color = SAMPLE_TEXTURE2D(_MainTex, sampler_MainTex, input.uv) * _Color * input.color;
                // Preserve GroundVisualisation's fixed shading and vertex gradient colors.
                color *= (color + max(dot(input.normalWS, float3(1,1,1)), 0.0)) / 2.0;
                color.rgb = AdjustColor(color.rgb);
                float contour = ContourMask(input.heightWS) * saturate(_LineColor.a);
                color.rgb = lerp(color.rgb, _LineColor.rgb, contour);
                return color;
            }
            ENDHLSL
        }
    }
    FallBack Off
}
