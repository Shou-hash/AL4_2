#include "Particle.hlsli"

// バースト発生リクエスト（ステージ裏面着地・弾命中など）
struct BurstSpawn
{
    float3 position;
    uint count;
    float3 direction;
    float spread;
    float4 color;
    float speedMin;
    float speedMax;
    float scaleMin;
    float scaleMax;
    float lifeTime;
    float gravity;
    float2 pad;
};

struct ParticleCSConstant
{
    float4x4 viewProjection;
    float4x4 billboard;
    float3 emitterPos;
    float deltaTime;
    uint maxParticles;
    uint frame;
    uint reset;
    float time;
    uint burstCursor;
    uint burstSpawnCount;
    float2 pad0;
    BurstSpawn bursts[8];
};

ConstantBuffer<ParticleCSConstant> gCSConstant : register(b0);
RWStructuredBuffer<ParticleForGPU> gParticles : register(u0);

// GPU用の簡易ハッシュ（疑似乱数）関数
float Hash(uint seed)
{
    seed = (seed ^ 61) ^ (seed >> 16);
    seed *= 9;
    seed = seed ^ (seed >> 4);
    seed *= 0x27d4eb2d;
    seed = seed ^ (seed >> 15);
    return float(seed) / 4294967295.0f;
}

// 粒ごと・フレームごとに変わる乱数 (0~1)
float Rand(uint index, uint salt)
{
    return Hash(index * 7919u + salt * 104729u + gCSConstant.frame * 2654435761u);
}

// 粒固有の固定乱数 (0~1)
float FixedRand(uint index, uint salt)
{
    return Hash(index * 16127u + salt * 31337u + 1337u);
}

// バーストパーティクル（ステージ裏面着地・弾命中）の生成
void EmitBurst(inout ParticleForGPU p, uint index, BurstSpawn b)
{
    float grainSeed = FixedRand(index, 9u);

    // 3D球面上のランダムベクトルと基準方向をブレンドして飛散方向を決定
    float theta = Rand(index, 1u) * 6.283185f;
    float cosPhi = Rand(index, 2u) * 2.0f - 1.0f;
    float sinPhi = sqrt(saturate(1.0f - cosPhi * cosPhi));
    float3 randDir = float3(sinPhi * cos(theta), cosPhi, sinPhi * sin(theta));

    float dirLen = length(b.direction);
    float3 baseDir = (dirLen > 0.001f) ? (b.direction / dirLen) : randDir;
    float3 shootDir = normalize(lerp(baseDir, randDir, b.spread));

    float speed = lerp(b.speedMin, b.speedMax, Rand(index, 3u));
    float s = lerp(b.scaleMin, b.scaleMax, Rand(index, 4u));

    p.position = b.position + randDir * (Rand(index, 5u) * 0.35f);
    p.velocity = shootDir * speed;
    p.currentTime = 0.0f;
    p.lifeTime = b.lifeTime * lerp(0.75f, 1.25f, Rand(index, 6u));
    p.scale = float3(s, s, b.color.a);
    p.color = float4(b.color.rgb, 1.0f);
    p.pad = 7.0f + grainSeed * 0.45f; // 7: バーストパーティクル
}

[numthreads(256, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint index = DTid.x;
    if (index >= gCSConstant.maxParticles)
        return;

    ParticleForGPU p = gParticles[index];
    bool reset = gCSConstant.reset != 0;

    if (reset)
    {
        p.currentTime = 0.0f;
        p.lifeTime = 0.0f;
        p.scale = float3(0.0f, 0.0f, 0.0f);
    }

    // 今フレームにリクエストされたバースト範囲に該当するスロットなら新規発生
    uint capacity = max(gCSConstant.maxParticles, 1u);
    uint cursor = gCSConstant.burstCursor;
    for (uint bIdx = 0u; bIdx < gCSConstant.burstSpawnCount; ++bIdx)
    {
        BurstSpawn b = gCSConstant.bursts[bIdx];
        uint offset = (index + capacity - (cursor % capacity)) % capacity;
        if (offset < b.count)
        {
            EmitBurst(p, index, b);
            break;
        }
        cursor += b.count;
    }

    // 生存中のバースト粒を更新
    if (p.lifeTime > 0.0f && p.currentTime < p.lifeTime)
    {
        float dt = gCSConstant.deltaTime;
        p.currentTime += dt;
        p.velocity *= pow(0.18f, dt);
        p.position += p.velocity * dt;

        float lifeRatio = saturate(p.currentTime / p.lifeTime);
        float fade = pow(1.0f - lifeRatio, 1.4f);
        float shrink = lerp(1.0f, 0.25f, lifeRatio);
        float rotAngle = FixedRand(index, 10u) * 6.283185f + p.currentTime * 3.0f;

        p.color.a = saturate(p.scale.z * fade);

        float sx = p.scale.x * shrink;
        float sy = p.scale.y * shrink;
        float c = cos(rotAngle);
        float s = sin(rotAngle);
        float4x4 localScaleRot = float4x4(
             sx * c, sx * s, 0.0f, 0.0f,
            -sy * s, sy * c, 0.0f, 0.0f,
             0.0f, 0.0f, 1.0f, 0.0f,
             0.0f, 0.0f, 0.0f, 1.0f
        );
        float4x4 worldMatrix = mul(localScaleRot, gCSConstant.billboard);
        worldMatrix[3] = float4(p.position, 1.0f);

        p.World = worldMatrix;
        p.WVP = mul(worldMatrix, gCSConstant.viewProjection);
    }
    else
    {
        // 寿命切れの粒はスケール0にして非表示
        p.lifeTime = 0.0f;
        p.color.a = 0.0f;
        p.World = float4x4(
            0.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        );
        p.WVP = p.World;
    }

    gParticles[index] = p;
}