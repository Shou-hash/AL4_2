#include "Particle.hlsli"

// RootParameter[1]: StructuredBuffer SRV (t0, Vertex)
StructuredBuffer<ParticleForGPU> gParticle : register(t0);

VertexShaderOutput main(VertexShaderInput input, uint instanceId : SV_InstanceID)
{
    VertexShaderOutput output;
    ParticleForGPU p = gParticle[instanceId];

    output.position = mul(input.position, p.WVP);
    output.texcoord = input.texcoord;
    output.color = p.color;
    output.kind = (uint) p.pad;
    output.seed = frac(p.pad);
    return output;
}