#include "ParticleSystem.h"
#include <cassert>
#include <cstring>
#include <d3dcompiler.h>

#pragma comment(lib, "d3dcompiler.lib")

using namespace KamataEngine;
using Microsoft::WRL::ComPtr;

namespace {

// ---- 調整用 ----
const float kHalfExtentX = 120.0f;            // プレイヤー中心の左右の広がり
const float kHalfExtentY = 60.0f;            // 上下の広がり
const float kZMin = -50.0f;                  // 手前(カメラ側)の端
const float kZMax = 50.0f;                   // 奥の端（前後に空間がある）
const float kSpeedMin = 0.5f;                // 落下速度[/frame]
const float kSpeedMax = 0.9f;
const float kColor[4] = {0.65f, 0.8f, 1.0f, 0.7f}; // 雨の色
const float kDropScale[3] = {0.08f, 0.6f, 1.0f};  // 板ポリ(-1〜1)の拡大: 幅0.1 x 長さ1.0 の縦長の粒

// GPU上の粒1個(32byte)。HLSLのParticleと一致させる
struct Particle {
	float pos[3];
	float speed;
	float scale;
	float pad[3];
};

// コンピュートシェーダの定数(16DWORD)。HLSLのcbufferと一致させる
struct ComputeParams {
	float center[3];
	uint32_t frame;
	float halfExtent[3];
	uint32_t reset;
	float zMin, zMax, speedMin, speedMax;
	uint32_t count;
	float pad[3];
};
static_assert(sizeof(ComputeParams) == 64);

// 描画用の定数(8DWORD)
struct DrawParams {
	float color[4];
	float scale[3];
	float pad;
};
static_assert(sizeof(DrawParams) == 32);

const char* kComputeSrc = R"HLSL(
cbuffer Params : register(b0) {
	float3 center;
	uint frame;
	float3 halfExtent;
	uint reset;
	float zMin;
	float zMax;
	float speedMin;
	float speedMax;
	uint count;
	float3 pad;
};

struct Particle {
	float3 pos;
	float speed;
	float scale;
	float3 pad;
};
RWStructuredBuffer<Particle> particles : register(u0);

uint Pcg(uint v) {
	uint s = v * 747796405u + 2891336453u;
	uint w = ((s >> ((s >> 28u) + 4u)) ^ s) * 277803737u;
	return (w >> 22u) ^ w;
}
float Rand(inout uint seed) {
	seed = Pcg(seed);
	return (seed >> 8) * (1.0 / 16777216.0);
}

[numthreads(256, 1, 1)]
void main(uint3 id : SV_DispatchThreadID) {
	if (id.x >= count) {
		return;
	}
	Particle p = particles[id.x];
	uint seed = id.x * 9781u + frame * 6271u + 1u;

	if (reset != 0) {
		float rx = Rand(seed);
		float ry = Rand(seed);
		float rz = Rand(seed);
		p.pos = float3(center.x + (rx * 2.0 - 1.0) * halfExtent.x, center.y + (ry * 2.0 - 1.0) * halfExtent.y, lerp(zMin, zMax, rz));
		p.speed = lerp(speedMin, speedMax, Rand(seed));
		p.scale = lerp(0.6, 1.4, Rand(seed));
	}

	// 落下
	p.pos.y -= p.speed;

	// X：プレイヤーの移動に合わせて、箱の反対側へループさせる
	float width = halfExtent.x * 2.0;
	float rel = p.pos.x - (center.x - halfExtent.x);
	rel -= floor(rel / width) * width;
	p.pos.x = center.x - halfExtent.x + rel;

	// Y：下限を下回ったら上へ戻し、X/Z/速度を作り直す
	if (p.pos.y < center.y - halfExtent.y) {
		float rx = Rand(seed);
		float rz = Rand(seed);
		p.pos.y = center.y + halfExtent.y;
		p.pos.x = center.x + (rx * 2.0 - 1.0) * halfExtent.x;
		p.pos.z = lerp(zMin, zMax, rz);
		p.speed = lerp(speedMin, speedMax, Rand(seed));
	}

	particles[id.x] = p;
}
)HLSL";

const char* kGraphicsSrc = R"HLSL(
#pragma pack_matrix(row_major)

cbuffer Camera : register(b0) {
	matrix view;
	matrix projection;
	float3 cameraPos;
};
cbuffer Draw : register(b1) {
	float4 color;
	float3 baseScale;
	float pad;
};

struct Particle {
	float3 pos;
	float speed;
	float scale;
	float3 pad;
};
StructuredBuffer<Particle> particles : register(t0);
Texture2D<float4> tex : register(t1);
SamplerState smp : register(s0);

struct VSOut {
	float4 svpos : SV_POSITION;
	float2 uv : TEXCOORD;
};

VSOut VSMain(float3 pos : POSITION, float3 normal : NORMAL, float2 uv : TEXCOORD, uint iid : SV_InstanceID) {
	Particle p = particles[iid];

	// カメラの右方向（水平）に幅、ワールドの上方向に長さを取る → どの視点でも縦長の雨粒に見える
	float3 right = float3(view[0][0], view[1][0], view[2][0]);
	right.y = 0.0;
	right = normalize(right);
	float3 world = p.pos + right * (pos.x * baseScale.x * p.scale) + float3(0.0, 1.0, 0.0) * (pos.y * baseScale.y * p.scale);

	VSOut o;
	o.svpos = mul(float4(world, 1.0), mul(view, projection));
	o.uv = uv;
	return o;
}

float4 PSMain(VSOut i) : SV_TARGET {
	float4 c = tex.Sample(smp, i.uv) * color;
	if (c.a < 0.01) {
		discard;
	}
	return c;
}
)HLSL";

ComPtr<ID3DBlob> Compile(const char* src, const char* entry, const char* target) {
	ComPtr<ID3DBlob> blob, error;
	HRESULT hr = D3DCompile(src, std::strlen(src), nullptr, nullptr, nullptr, entry, target, D3DCOMPILE_ENABLE_STRICTNESS, 0, &blob, &error);
	if (FAILED(hr)) {
		if (error) {
			OutputDebugStringA(static_cast<const char*>(error->GetBufferPointer()));
		}
		assert(false && "パーティクルのシェーダコンパイルに失敗");
	}
	return blob;
}

ComPtr<ID3D12RootSignature> CreateRootSignature(ID3D12Device* device, const D3D12_ROOT_SIGNATURE_DESC& desc) {
	ComPtr<ID3DBlob> blob, error;
	HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1_0, &blob, &error);
	if (FAILED(hr) && error) {
		OutputDebugStringA(static_cast<const char*>(error->GetBufferPointer()));
	}
	assert(SUCCEEDED(hr));
	ComPtr<ID3D12RootSignature> rootSignature;
	hr = device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
	assert(SUCCEEDED(hr));
	return rootSignature;
}

} // namespace

void ParticleSystem::Initialize(Model* model) {
	assert(model);
	model_ = model;
	ID3D12Device* device = DirectXCommon::GetInstance()->GetDevice();
	HRESULT hr;

	// ---- 粒のバッファ（GPU専用。初期値はコンピュートシェーダが作る）----
	{
		CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_DEFAULT);
		CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(Particle) * kParticleCount, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		hr = device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&particleBuffer_));
		assert(SUCCEEDED(hr));
	}

	// ---- コンピュート ----
	{
		CD3DX12_ROOT_PARAMETER params[2];
		params[0].InitAsConstants(sizeof(ComputeParams) / 4, 0);
		params[1].InitAsUnorderedAccessView(0);
		CD3DX12_ROOT_SIGNATURE_DESC desc(_countof(params), params, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);
		computeRootSignature_ = CreateRootSignature(device, desc);

		ComPtr<ID3DBlob> cs = Compile(kComputeSrc, "main", "cs_5_1");
		D3D12_COMPUTE_PIPELINE_STATE_DESC pso{};
		pso.pRootSignature = computeRootSignature_.Get();
		pso.CS = CD3DX12_SHADER_BYTECODE(cs.Get());
		hr = device->CreateComputePipelineState(&pso, IID_PPV_ARGS(&computePipeline_));
		assert(SUCCEEDED(hr));
	}

	// ---- 描画 ----
	{
		CD3DX12_DESCRIPTOR_RANGE range;
		range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1); // t1: テクスチャ
		CD3DX12_ROOT_PARAMETER params[4];
		params[0].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_VERTEX);  // b0: カメラ
		params[1].InitAsShaderResourceView(0, 0, D3D12_SHADER_VISIBILITY_VERTEX);  // t0: 粒
		params[2].InitAsConstants(sizeof(DrawParams) / 4, 1);                      // b1: 色・大きさ
		params[3].InitAsDescriptorTable(1, &range, D3D12_SHADER_VISIBILITY_PIXEL); // t1
		CD3DX12_STATIC_SAMPLER_DESC sampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR);
		sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		CD3DX12_ROOT_SIGNATURE_DESC desc(_countof(params), params, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
		graphicsRootSignature_ = CreateRootSignature(device, desc);

		ComPtr<ID3DBlob> vs = Compile(kGraphicsSrc, "VSMain", "vs_5_1");
		ComPtr<ID3DBlob> ps = Compile(kGraphicsSrc, "PSMain", "ps_5_1");

		// 頂点は Mesh::VertexPosNormalUv と同じ並び
		D3D12_INPUT_ELEMENT_DESC layout[] = {
		    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		    {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		};

		D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
		pso.pRootSignature = graphicsRootSignature_.Get();
		pso.VS = CD3DX12_SHADER_BYTECODE(vs.Get());
		pso.PS = CD3DX12_SHADER_BYTECODE(ps.Get());
		pso.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		D3D12_RENDER_TARGET_BLEND_DESC& rt = pso.BlendState.RenderTarget[0];
		rt.BlendEnable = TRUE;
		rt.SrcBlend = D3D12_BLEND_SRC_ALPHA;
		rt.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		rt.BlendOp = D3D12_BLEND_OP_ADD;
		rt.SrcBlendAlpha = D3D12_BLEND_ONE;
		rt.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
		rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;
		rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		pso.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
		pso.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		pso.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		pso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // 深度は見るだけ(半透明なので書かない)
		pso.InputLayout = {layout, _countof(layout)};
		pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pso.NumRenderTargets = 1;
		// ※ エンジンのバックバッファ/深度のフォーマットに合わせる（合わない場合はここを変更）
		pso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		pso.DSVFormat = DXGI_FORMAT_D32_FLOAT;
		pso.SampleDesc.Count = 1;
		hr = device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&graphicsPipeline_));
		assert(SUCCEEDED(hr));
	}
}

void ParticleSystem::Draw(const Camera& camera, const Vector3& center) {
	ID3D12GraphicsCommandList* cl = DirectXCommon::GetInstance()->GetCommandList();
	ID3D12Resource* buffer = particleBuffer_.Get();

	// ---- 1. コンピュートで全粒を更新 ----
	ComputeParams params{};
	params.center[0] = center.x;
	params.center[1] = center.y;
	params.center[2] = center.z;
	params.frame = frame_++;
	params.halfExtent[0] = kHalfExtentX;
	params.halfExtent[1] = kHalfExtentY;
	params.halfExtent[2] = (kZMax - kZMin) * 0.5f;
	params.reset = reset_ ? 1u : 0u;
	params.zMin = kZMin;
	params.zMax = kZMax;
	params.speedMin = kSpeedMin;
	params.speedMax = kSpeedMax;
	params.count = kParticleCount;
	reset_ = false;

	cl->SetComputeRootSignature(computeRootSignature_.Get());
	cl->SetPipelineState(computePipeline_.Get());
	cl->SetComputeRoot32BitConstants(0, sizeof(ComputeParams) / 4, &params, 0);
	cl->SetComputeRootUnorderedAccessView(1, buffer->GetGPUVirtualAddress());
	cl->Dispatch((kParticleCount + 255) / 256, 1, 1);

	// UAV(書き込み) → 頂点シェーダから読む状態へ
	CD3DX12_RESOURCE_BARRIER toRead = CD3DX12_RESOURCE_BARRIER::Transition(buffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	cl->ResourceBarrier(1, &toRead);

	// ---- 2. 全粒を1回のインスタンス描画で描く ----
	DrawParams draw{};
	std::memcpy(draw.color, kColor, sizeof(kColor));
	std::memcpy(draw.scale, kDropScale, sizeof(kDropScale));

	cl->SetGraphicsRootSignature(graphicsRootSignature_.Get());
	cl->SetPipelineState(graphicsPipeline_.Get());
	cl->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cl->SetGraphicsRootConstantBufferView(0, camera.GetConstBuffer()->GetGPUVirtualAddress());
	cl->SetGraphicsRootShaderResourceView(1, buffer->GetGPUVirtualAddress());
	cl->SetGraphicsRoot32BitConstants(2, sizeof(DrawParams) / 4, &draw, 0);

	for (const auto& mesh : model_->GetMeshes()) {
		Material* material = mesh->GetMaterial();
		assert(material);
		TextureManager::GetInstance()->SetGraphicsRootDescriptorTable(cl, 3, material->GetTextureHandle());
		cl->IASetVertexBuffers(0, 1, &mesh->GetVBView());
		cl->IASetIndexBuffer(&mesh->GetIBView());
		cl->DrawIndexedInstanced(static_cast<UINT>(mesh->GetIndices().size()), kParticleCount, 0, 0, 0);
	}

	// 次フレームのコンピュートのため UAV に戻す
	CD3DX12_RESOURCE_BARRIER toWrite = CD3DX12_RESOURCE_BARRIER::Transition(buffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	cl->ResourceBarrier(1, &toWrite);
}
