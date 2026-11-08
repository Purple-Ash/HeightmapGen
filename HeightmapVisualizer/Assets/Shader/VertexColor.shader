Shader "Custom/VertexColorVF"
{
    Properties
    {
        _Color ("Color", Color) = (1,1,1,1)
        _MainTex ("Texture", 2D) = "white" {}
        _Glossiness ("Smoothness", Range(0,1)) = 0.5
        _Metallic ("Metallic", Range(0,1)) = 0.0

        [Header(Color Adjustments)]
        _Saturation ("Saturation", Range(0,2)) = 1.0
        _Contrast ("Contrast", Range(0,2)) = 1.0
        _Brightness ("Brightness", Range(0,2)) = 1.0
    }

    SubShader
    {
        Tags { "RenderType"="Opaque" }
        LOD 200

        Pass
        {
            CGPROGRAM

            #pragma vertex vert
            #pragma fragment frag

            #include "UnityCG.cginc"

            sampler2D _MainTex;
            float4 _MainTex_ST;

            fixed4 _Color;
            half _Glossiness;
            half _Metallic;
            float _Saturation;
            float _Contrast;
            float _Brightness;

            struct appdata
            {
                float4 vertex : POSITION;
                float3 normal : NORMAL;
                float2 uv : TEXCOORD0;
                float4 color : COLOR;   // vertex color
            };

            struct v2f
            {
                float2 uv : TEXCOORD0;
                float4 pos : SV_POSITION;
                float4 color : COLOR;
                float3 worldNormal : TEXCOORD1;
            };

            v2f vert(appdata v)
            {
                v2f o;
                o.pos = UnityObjectToClipPos(v.vertex);
                o.uv = TRANSFORM_TEX(v.uv, _MainTex);
                o.color = v.color;
                o.worldNormal = UnityObjectToWorldNormal(v.normal);

                return o;
            }

            float3 AdjustColor(float3 color)
            {
                float luminance = dot(color, float3(0.2126, 0.7152, 0.0722));
                color = lerp(luminance.xxx, color, _Saturation);
                color = (color - 0.5) * _Contrast + 0.5;
                // Keep HDR highlights; only clamp negative values from contrast/saturation.
                return max(color, 0.0) * _Brightness;
            }

            float4 frag(v2f i) : SV_Target
            {
                float4 tex = tex2D(_MainTex, i.uv);
                float4 col = tex * _Color * i.color;
                col *= (col +  max(dot(i.worldNormal, float3(1,1,1)),0.0) )/2.0;
                // Grade the final shaded RGB; these controls do not affect alpha.
                col.rgb = AdjustColor(col.rgb);
                return col;
            }

            ENDCG
        }
    }

    FallBack "Diffuse"
}