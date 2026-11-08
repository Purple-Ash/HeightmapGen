Shader "Custom/TerrainAtmosphere"
{
    Properties
    {
        _Color ("Color", Color) = (1,1,1,1)
        _MainTex ("Texture", 2D) = "white" {}

        [Header(Color Adjustments)]
        _Saturation ("Saturation", Range(0,2)) = 1
        _Contrast ("Contrast", Range(0,2)) = 1
        _Brightness ("Brightness", Range(0,2)) = 1

        [Header(Distance Haze)]
        _DistanceFogColor ("Haze Color", Color) = (0.42,0.58,0.76,1)
        _DistanceFogStart ("Start Distance (World Units)", Float) = 100
        _DistanceFogDensity ("Haze Density", Range(0,0.02)) = 0.002
        _DistanceFogOpacity ("Maximum Haze Opacity", Range(0,1)) = 0.85

        [Header(Low Ground Fog)]
        _GroundFogColor ("Ground Fog Color", Color) = (0.12,0.16,0.22,1)
        _GroundFogBaseHeight ("Full Density Height (World Y)", Float) = 15
        _GroundFogHeight ("Fade Height Above Base (World Units)", Float) = 35
        _GroundFogDensity ("Ground Fog Density", Range(0,0.1)) = 0.01
        _GroundFogOpacity ("Maximum Ground Fog Opacity", Range(0,1)) = 0.25
    }

    SubShader
    {
        Tags { "RenderType"="Opaque" "Queue"="Geometry" "RenderPipeline"="UniversalPipeline" }

        HLSLINCLUDE
        #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Core.hlsl"

        TEXTURE2D(_MainTex);
        SAMPLER(sampler_MainTex);

        CBUFFER_START(UnityPerMaterial)
            float4 _MainTex_ST;
            float4 _Color;
            float4 _DistanceFogColor;
            float4 _GroundFogColor;
            float _Saturation;
            float _Contrast;
            float _Brightness;
            float _DistanceFogStart;
            float _DistanceFogDensity;
            float _DistanceFogOpacity;
            float _GroundFogBaseHeight;
            float _GroundFogHeight;
            float _GroundFogDensity;
            float _GroundFogOpacity;
        CBUFFER_END
        ENDHLSL

        Pass
        {
            Name "TerrainAtmosphere"
            Tags { "LightMode"="UniversalForwardOnly" }
            ZWrite On
            Cull Back

            HLSLPROGRAM
            #pragma vertex vert
            #pragma fragment frag
            #pragma target 3.0
            #pragma multi_compile _ _MAIN_LIGHT_SHADOWS _MAIN_LIGHT_SHADOWS_CASCADE _MAIN_LIGHT_SHADOWS_SCREEN
            #pragma multi_compile_fragment _ _SHADOWS_SOFT _SHADOWS_SOFT_LOW _SHADOWS_SOFT_MEDIUM _SHADOWS_SOFT_HIGH
            #pragma multi_compile_fragment _ _SCREEN_SPACE_OCCLUSION
            #pragma multi_compile _ _FORWARD_PLUS

            #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Lighting.hlsl"

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
                float3 positionWS : TEXCOORD1;
                float3 normalWS : TEXCOORD2;
                float4 color : COLOR;
            };

            Varyings vert(Attributes input)
            {
                Varyings output;
                output.positionCS = TransformObjectToHClip(input.positionOS.xyz);
                output.positionWS = TransformObjectToWorld(input.positionOS.xyz);
                output.normalWS = TransformObjectToWorldNormal(input.normalOS);
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

            // Integrate density along the view ray, rather than tinting by surface height alone.
            // Density is 1 below the base and fades linearly to 0 at base + fade height.
            float AverageGroundDensity(float cameraHeight, float surfaceHeight)
            {
                float fadeHeight = max(_GroundFogHeight, 0.001);
                float low = (min(cameraHeight, surfaceHeight) - _GroundFogBaseHeight) / fadeHeight;
                float high = (max(cameraHeight, surfaceHeight) - _GroundFogBaseHeight) / fadeHeight;
                float span = high - low;

                // Stable for horizontal and almost horizontal rays, including inside the layer.
                if (span < 0.001)
                    return saturate(1.0 - 0.5 * (low + high));

                float belowBase = max(min(high, 0.0) - low, 0.0);
                float rampStart = saturate(low);
                float rampEnd = saturate(high);
                float rampArea = (rampEnd - rampStart) * (1.0 - 0.5 * (rampStart + rampEnd));
                return saturate((belowBase + rampArea) / span);
            }

            float FogAmount(float distance, float density, float opacity)
            {
                return saturate(opacity) * (1.0 - exp(-max(distance, 0.0) * max(density, 0.0)));
            }

            float4 frag(Varyings input) : SV_Target
            {
                float4 color = SAMPLE_TEXTURE2D(_MainTex, sampler_MainTex, input.uv) * _Color * input.color;
                float3 normalWS = NormalizeNormalPerPixel(input.normalWS);
                // Screen-space shadows need screen coordinates; other variants sample the shadow atlas.
                #if defined(_MAIN_LIGHT_SHADOWS_SCREEN)
                    float4 shadowCoord = ComputeScreenPos(TransformWorldToHClip(input.positionWS));
                #else
                    float4 shadowCoord = TransformWorldToShadowCoord(input.positionWS);
                #endif
                // The position-aware overload also fades shadows at the pipeline's shadow distance.
                Light sun = GetMainLight(shadowCoord, input.positionWS, half4(1,1,1,1));
                AmbientOcclusionFactor occlusion = GetScreenSpaceAmbientOcclusion(
                    GetNormalizedScreenSpaceUV(input.positionCS));
                float3 ambient = SampleSH(normalWS) * occlusion.indirectAmbientOcclusion;
                float3 direct = LightingLambert(sun.color, sun.direction, normalWS)
                    * sun.distanceAttenuation * sun.shadowAttenuation * occlusion.directAmbientOcclusion;
                color.rgb *= ambient + direct;
                color.rgb = AdjustColor(color.rgb);

                float3 cameraPosition = GetCameraPositionWS();
                float viewDistance = distance(cameraPosition, input.positionWS);
                float groundPath = viewDistance * AverageGroundDensity(cameraPosition.y, input.positionWS.y);
                float groundFog = FogAmount(groundPath, _GroundFogDensity, _GroundFogOpacity);
                color.rgb = lerp(color.rgb, _GroundFogColor.rgb, groundFog);

                // Distant features converge toward blue even when they lie in low ground fog.
                float hazeDistance = max(viewDistance - max(_DistanceFogStart, 0.0), 0.0);
                float haze = FogAmount(hazeDistance, _DistanceFogDensity, _DistanceFogOpacity);
                color.rgb = lerp(color.rgb, _DistanceFogColor.rgb, haze);
                return color;
            }
            ENDHLSL
        }

        // The terrain is opaque, so URP's standard passes need no texture/alpha clipping setup.
        Pass
        {
            Name "ShadowCaster"
            Tags { "LightMode"="ShadowCaster" }
            ZWrite On
            ZTest LEqual
            ColorMask 0
            Cull Back

            HLSLPROGRAM
            #pragma target 3.0
            #pragma vertex ShadowPassVertex
            #pragma fragment ShadowPassFragment
            #pragma multi_compile_vertex _ _CASTING_PUNCTUAL_LIGHT_SHADOW
            #include "Packages/com.unity.render-pipelines.universal/Shaders/ShadowCasterPass.hlsl"
            ENDHLSL
        }

        Pass
        {
            Name "DepthOnly"
            Tags { "LightMode"="DepthOnly" }
            ZWrite On
            ColorMask R
            Cull Back

            HLSLPROGRAM
            #pragma target 3.0
            #pragma vertex DepthOnlyVertex
            #pragma fragment DepthOnlyFragment
            #include "Packages/com.unity.render-pipelines.universal/Shaders/DepthOnlyPass.hlsl"
            ENDHLSL
        }

        Pass
        {
            Name "DepthNormalsOnly"
            Tags { "LightMode"="DepthNormalsOnly" }
            ZWrite On
            Cull Back

            HLSLPROGRAM
            #pragma target 3.0
            #pragma vertex DepthNormalsVertex
            #pragma fragment DepthNormalsFragment
            #pragma multi_compile_fragment _ _GBUFFER_NORMALS_OCT
            #include "Packages/com.unity.render-pipelines.universal/Shaders/DepthNormalsPass.hlsl"
            ENDHLSL
        }
    }
    FallBack Off
}
