#include "Particle.hlsli"

// RootParameter[0]: CBV (b0)
ConstantBuffer<Material> gMaterial : register(b0);

// RootParameter[2]: Texture SRV (t0)
Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    float2 uv = input.texcoord * 2.0f - 1.0f; // -1 ~ 1
    float dist = length(uv);
    if (dist >= 1.0f)
    {
        discard;
    }

    float4 tex = gTexture.Sample(gSampler, input.texcoord);

    // 眩しさを抑えたソフトな丸粒（外周を素早くフェード）
    float core = pow(saturate(1.0f - dist), 2.5f);
    float glow = exp(-dist * dist * 6.0f);
    float shapeAlpha = saturate(core * 0.55f + glow * 0.35f);
    float3 shade = lerp(float3(0.95f, 0.95f, 0.95f), float3(1.05f, 1.05f, 1.05f), glow);

    if (shapeAlpha * tex.a <= 0.004f)
    {
        discard;
    }

    float4 finalColor = gMaterial.color * input.color;
    finalColor.rgb *= tex.rgb * shade;
    finalColor.a *= saturate(shapeAlpha) * tex.a;
    output.color = finalColor;
    return output;
}