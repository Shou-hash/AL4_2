#pragma pack_matrix(row_major) // ★ C++ の行列(行優先)と、同じ並びで読む。これが無いと、定数バッファの viewProjection・billboard が転置されて、粒が正しい位置に出ない

struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float4 color : COLOR0;
    nointerpolation uint kind : TEXCOORD1; // 7: ステージ裏面・弾命中バースト
    float seed : TEXCOORD2;
};

struct VertexShaderInput
{
    float4 position : POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL;
};

struct Material
{
    float4 color;
    uint enableLighting;
    float3 pad;
    float4x4 uvTransform;
};

// GPU側でデータ更新と描画用データを統一して保持する構造体（合計 192 byte）
struct ParticleForGPU
{
    float4x4 WVP;
    float4x4 World;
    float4 color;
    float3 position;
    float currentTime;
    float3 velocity;
    float lifeTime;
    float3 scale;
    float pad; // 整数部: 種類(7:バースト), 小数部: 粒ごとの固有シード(0~0.99)
};