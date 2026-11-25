#include "EffectParticleSystem.h"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <d3dcompiler.h>
#include <filesystem>
#include <string>

#pragma comment(lib, "d3dcompiler.lib")

using namespace KamataEngine;
using namespace KamataEngine::MathUtility;
using Microsoft::WRL::ComPtr;

namespace {

struct ParticleForGPU {
	Matrix4x4 WVP;     // 64
	Matrix4x4 World;   // 64 (累計 128)
	Vector4 color;     // 16 (累計 144)
	Vector3 position;  // 12
	float currentTime; //  4 (累計 160)
	Vector3 velocity;  // 12
	float lifeTime;    //  4 (累計 176)
	Vector3 scale;     // 12
	float pad;         //  4 (累計 192)
};
static_assert(sizeof(ParticleForGPU) == 192);

struct BurstSpawnGPU {
	float position[3];  // 12
	uint32_t count;     //  4 (16)
	float direction[3]; // 12
	float spread;       //  4 (32)
	float color[4];     // 16 (48)
	float speedMin;     //  4
	float speedMax;     //  4
	float scaleMin;     //  4
	float scaleMax;     //  4 (64)
	float lifeTime;     //  4
	float gravity;      //  4
	float pad[2];       //  8 (80)
};
static_assert(sizeof(BurstSpawnGPU) == 80);

struct ParticleCSConstant {
	Matrix4x4 viewProjection; // 64
	Matrix4x4 billboard;      // 64 (128)
	Vector3 emitterPos;       // 12
	float deltaTime;          //  4 (144)
	uint32_t maxParticles;    //  4
	uint32_t frame;           //  4
	uint32_t reset;           //  4
	float time;               //  4 (160)
	uint32_t burstCursor;     //  4
	uint32_t burstSpawnCount; //  4
	float pad0[2];            //  8 (176)
	BurstSpawnGPU bursts[8];  // 80 * 8 = 640 (累計 816)
};
static_assert(sizeof(ParticleCSConstant) == 816);

struct MaterialConstant {
	Vector4 color;
	uint32_t enableLighting;
	float pad[3];
	Matrix4x4 uvTransform;
};

std::wstring ResolveShaderPath(const std::wstring& fileName) {
	// ファイル名の区切りが「.」(Particle.CS.hlsl)でも「_」(Particle_CS.hlsl)でも、見つかるようにする
	std::wstring altName = fileName;
	const size_t lastDot = altName.find_last_of(L'.');
	for (size_t i = 0; i < lastDot; ++i) {
		if (altName[i] == L'.') {
			altName[i] = L'_';
		}
	}
	const std::wstring candidates[] = {
	    L"Resources/shaders/" + fileName, L"shaders/" + fileName, fileName, L"Resources/shaders/" + altName, L"shaders/" + altName, altName,
	};
	for (const auto& path : candidates) {
		if (std::filesystem::exists(path)) {
			return path;
		}
	}
	return candidates[0];
}

ComPtr<ID3DBlob> CompileShaderFromFile(const std::wstring& fileName, const char* entryPoint, const char* target) {
	std::wstring filePath = ResolveShaderPath(fileName);
	ComPtr<ID3DBlob> blob;
	ComPtr<ID3DBlob> errorBlob;
	HRESULT hr = D3DCompileFromFile(filePath.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, entryPoint, target, D3DCOMPILE_ENABLE_STRICTNESS, 0, &blob, &errorBlob);
	if (FAILED(hr)) {
		if (errorBlob) {
			OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
		} else {
			std::wstring msg = L"シェーダファイルが見つかりません: " + filePath + L"\n";
			OutputDebugStringW(msg.c_str());
		}
		assert(false && "外部シェーダファイルのコンパイルに失敗しました。出力ウィンドウのエラー内容を確認してください。");
	}
	return blob;
}

ComPtr<ID3D12RootSignature> CreateRootSignature(ID3D12Device* device, const D3D12_ROOT_SIGNATURE_DESC& desc) {
	ComPtr<ID3DBlob> blob;
	ComPtr<ID3DBlob> error;
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

void EffectParticleSystem::Initialize(Model* model) {
	assert(model);
	model_ = model;

	textureHandle_ = TextureManager::Load("white1x1.png"); // 予備(モデルに画像が無いときだけ使う)。ふだんは、Resources/particle のモデルの画像を貼る

	ID3D12Device* device = DirectXCommon::GetInstance()->GetDevice();
	HRESULT hr;

	// ---- 1. パーティクル用 StructuredBuffer（GPU専用）----
	{
		CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_DEFAULT);
		CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(ParticleForGPU) * kTotalParticles, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		hr = device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&particleBuffer_));
		assert(SUCCEEDED(hr));
	}

	// ---- 2. コンピュート用 ConstantBuffer（Particle.CS.hlsl の b0 用）----
	{
		UINT cbSize = (sizeof(ParticleCSConstant) + 255) & ~255u;
		CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_UPLOAD);
		CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(cbSize);
		hr = device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&csConstantBuffer_));
		assert(SUCCEEDED(hr));
	}

	// ---- 3. マテリアル用 ConstantBuffer（Particle.PS.hlsl の b0 用）----
	{
		UINT cbSize = (sizeof(MaterialConstant) + 255) & ~255u;
		CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_UPLOAD);
		CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(cbSize);
		hr = device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&materialBuffer_));
		assert(SUCCEEDED(hr));

		MaterialConstant* mapped = nullptr;
		materialBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&mapped));
		mapped->color = {1.0f, 1.0f, 1.0f, 1.0f};
		mapped->enableLighting = 0;
		mapped->uvTransform = MakeIdentityMatrix();
		materialBuffer_->Unmap(0, nullptr);
	}

	// ---- 4. コンピュートパイプライン（Particle.CS.hlsl）----
	{
		CD3DX12_ROOT_PARAMETER params[2];
		params[0].InitAsConstantBufferView(0);  // b0: gCSConstant
		params[1].InitAsUnorderedAccessView(0); // u0: gParticles
		CD3DX12_ROOT_SIGNATURE_DESC desc(_countof(params), params, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);
		computeRootSignature_ = CreateRootSignature(device, desc);

		ComPtr<ID3DBlob> cs = CompileShaderFromFile(L"Particle.CS.hlsl", "main", "cs_5_1");
		D3D12_COMPUTE_PIPELINE_STATE_DESC pso{};
		pso.pRootSignature = computeRootSignature_.Get();
		pso.CS = CD3DX12_SHADER_BYTECODE(cs.Get());
		hr = device->CreateComputePipelineState(&pso, IID_PPV_ARGS(&computePipeline_));
		assert(SUCCEEDED(hr));
	}

	// ---- 5. グラフィックスパイプライン（Particle.VS.hlsl / Particle.PS.hlsl）----
	{
		CD3DX12_DESCRIPTOR_RANGE range;
		range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // t0 (Pixel): gTexture

		CD3DX12_ROOT_PARAMETER params[3];
		params[0].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_PIXEL);   // b0 (Pixel): gMaterial
		params[1].InitAsShaderResourceView(0, 0, D3D12_SHADER_VISIBILITY_VERTEX);  // t0 (Vertex): gParticle
		params[2].InitAsDescriptorTable(1, &range, D3D12_SHADER_VISIBILITY_PIXEL); // t0 (Pixel): gTexture

		CD3DX12_STATIC_SAMPLER_DESC sampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR);
		sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		CD3DX12_ROOT_SIGNATURE_DESC desc(_countof(params), params, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
		graphicsRootSignature_ = CreateRootSignature(device, desc);

		ComPtr<ID3DBlob> vs = CompileShaderFromFile(L"Particle.VS.hlsl", "main", "vs_5_1");
		ComPtr<ID3DBlob> ps = CompileShaderFromFile(L"Particle.PS.hlsl", "main", "ps_5_1");

		// Mesh::VertexPosNormalUv と同じ頂点並び（POSITION -> NORMAL -> TEXCOORD）
		D3D12_INPUT_ELEMENT_DESC layout[] = {
		    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		    {"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		};

		D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
		pso.pRootSignature = graphicsRootSignature_.Get();
		pso.VS = CD3DX12_SHADER_BYTECODE(vs.Get());
		pso.PS = CD3DX12_SHADER_BYTECODE(ps.Get());

		pso.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		D3D12_RENDER_TARGET_BLEND_DESC& rt = pso.BlendState.RenderTarget[0];
		rt.BlendEnable = TRUE;
		rt.SrcBlend = D3D12_BLEND_SRC_ALPHA;
		rt.DestBlend = D3D12_BLEND_INV_SRC_ALPHA; // ★ 加算合成から通常アルファブレンドへ変更（白飛び防止）
		rt.BlendOp = D3D12_BLEND_OP_ADD;
		rt.SrcBlendAlpha = D3D12_BLEND_ONE;
		rt.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
		rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;
		rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

		pso.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
		pso.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

		pso.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		pso.DepthStencilState.DepthEnable = FALSE;
		pso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

		pso.InputLayout = {layout, _countof(layout)};
		pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pso.NumRenderTargets = 1;
		pso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		pso.DSVFormat = DXGI_FORMAT_D32_FLOAT;
		pso.SampleDesc.Count = 1;

		hr = device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&graphicsPipeline_));
		assert(SUCCEEDED(hr));
	}
}

void EffectParticleSystem::UpdateImGui(const Vector3& playerPos, const Vector3& cameraOffset) {
	ImGui::Begin("EffectParticleSystem");

	// ★ チェックONでカメラオフセットをパーティクル発生座標にする
	if (ImGui::Checkbox("Camera Offset = Particle Pos", &useCameraOffsetPos_)) {
		if (useCameraOffsetPos_) {
			// チェックを入れた瞬間に自動で連続発生もONにしてすぐ確認できるようにする
			debugAutoBurst_ = true;
		}
	}

	Vector3 spawnPos{};
	if (useCameraOffsetPos_) {
		// カメラのターゲットオフセット（playerPos + cameraOffset）を基準座標にする
		spawnPos = {
		    playerPos.x + cameraOffset.x + debugOffset_[0],
		    playerPos.y + cameraOffset.y + debugOffset_[1],
		    playerPos.z + cameraOffset.z + debugOffset_[2],
		};
	} else {
		spawnPos = {
		    playerPos.x + debugOffset_[0],
		    playerPos.y + debugOffset_[1],
		    playerPos.z + debugOffset_[2],
		};
	}
	lastSpawnPos_ = spawnPos;

	ImGui::Text("Player Pos: (%.2f, %.2f, %.2f)", playerPos.x, playerPos.y, playerPos.z);
	ImGui::Text("Camera Offset: (%.2f, %.2f, %.2f)", cameraOffset.x, cameraOffset.y, cameraOffset.z);
	ImGui::Text("Particle Spawn Pos: (%.2f, %.2f, %.2f)", spawnPos.x, spawnPos.y, spawnPos.z);
	ImGui::Text("Total Emitted: %u | Cursor: %u / %u", totalEmittedCount_, burstCursor_, kTotalParticles);
	
	ImGui::Separator();
	ImGui::DragFloat3("Additional Offset", debugOffset_, 0.2f, -50.0f, 50.0f);
	ImGui::SliderInt("Burst Count", &debugBurstCount_, 4, 128);
	ImGui::SliderFloat("Scale Max", &debugScaleMax_, 0.1f, 2.0f);
	ImGui::SliderFloat("Alpha (Brightness)", &debugAlpha_, 0.05f, 1.0f);
	ImGui::SliderFloat("Speed Max", &debugSpeedMax_, 1.0f, 20.0f);
	ImGui::SliderFloat("Life Time", &debugLifeTime_, 0.1f, 1.5f);

	ImGui::Separator();

	if (ImGui::Button("Emit Bullet Hit (Yellow)", ImVec2(220.0f, 30.0f))) {
		EmitBulletHit(spawnPos);
	}

	if (ImGui::Button("Emit Stage Back (Cyan)", ImVec2(220.0f, 30.0f))) {
		EmitStageBack(spawnPos);
	}

	if (ImGui::Button("Emit Both Now!", ImVec2(220.0f, 30.0f))) {
		EmitBulletHit(spawnPos);
		EmitStageBack(spawnPos);
	}

	ImGui::Checkbox("Auto Burst (Every 0.3s)", &debugAutoBurst_);
	if (debugAutoBurst_) {
		debugAutoTimer_ += 1.0f / 60.0f;
		if (debugAutoTimer_ >= 0.3f) {
			debugAutoTimer_ = 0.0f;
			EmitBulletHit(spawnPos);
			EmitStageBack(spawnPos);
		}
	} else {
		debugAutoTimer_ = 0.0f;
	}

	if (ImGui::Button("Reset All Particles")) {
		reset_ = true;
		burstCursor_ = 0;
		totalEmittedCount_ = 0;
		pendingBursts_.clear();
	}

	ImGui::End();
}

void EffectParticleSystem::EmitStageBack(const Vector3& backPos, int count, float directionZ) {
	if (pendingBursts_.size() >= 8) {
		return;
	}
	BurstSpawn b{};
	b.position[0] = backPos.x;
	b.position[1] = backPos.y;
	b.position[2] = backPos.z;
	b.count = static_cast<uint32_t>(count > 0 ? count : debugBurstCount_);
	b.direction[0] = 0.0f;
	b.direction[1] = 0.0f;
	b.direction[2] = directionZ;
	b.spread = 0.85f;
	b.color[0] = 0.45f;
	b.color[1] = 0.85f;
	b.color[2] = 1.00f;
	b.color[3] = debugAlpha_; // ★ 透明度を抑えて画面が見えなくならないようにする
	b.speedMin = 3.0f;
	b.speedMax = debugSpeedMax_;
	b.scaleMin = 0.15f;
	b.scaleMax = debugScaleMax_;
	b.lifeTime = debugLifeTime_;
	pendingBursts_.push_back(b);
	totalEmittedCount_ += b.count;
}

void EffectParticleSystem::EmitBulletHit(const Vector3& hitPos, int count) {
	if (pendingBursts_.size() >= 8) {
		return;
	}
	BurstSpawn b{};
	b.position[0] = hitPos.x;
	b.position[1] = hitPos.y;
	b.position[2] = hitPos.z;
	b.count = static_cast<uint32_t>(count > 0 ? count : debugBurstCount_);
	b.direction[0] = 0.0f;
	b.direction[1] = 0.0f;
	b.direction[2] = 0.0f;
	b.spread = 1.0f;
	b.color[0] = 1.00f;
	b.color[1] = 0.80f;
	b.color[2] = 0.25f;
	b.color[3] = debugAlpha_; // ★ 透明度を抑えて画面が見えなくならないようにする
	b.speedMin = 4.0f;
	b.speedMax = debugSpeedMax_;
	b.scaleMin = 0.15f;
	b.scaleMax = debugScaleMax_;
	b.lifeTime = debugLifeTime_;
	pendingBursts_.push_back(b);
	totalEmittedCount_ += b.count;
}

void EffectParticleSystem::Draw(const Camera& camera, const Vector3& center) {
	DirectXCommon* dxCommon = DirectXCommon::GetInstance();
	ID3D12GraphicsCommandList* cl = dxCommon->GetCommandList();
	ID3D12Resource* buffer = particleBuffer_.Get();

	const float dt = 1.0f / 60.0f;
	time_ += dt;

	// カメラのビュー行列の回転成分を転置して3Dビルボード回転行列を作成
	Matrix4x4 billboard = MakeIdentityMatrix();
	for (int r = 0; r < 3; ++r) {
		for (int c = 0; c < 3; ++c) {
			billboard.m[r][c] = camera.matView.m[c][r];
		}
	}

	// ---- 1. コンピュートシェーダ（Particle.CS.hlsl）用定数バッファを更新 ----
	ParticleCSConstant params{};
	params.viewProjection = camera.matView * camera.matProjection;
	params.billboard = billboard;
	params.emitterPos = useCameraOffsetPos_ ? lastSpawnPos_ : center;
	params.deltaTime = dt;
	params.maxParticles = kTotalParticles;
	params.frame = frame_++;
	params.reset = reset_ ? 1u : 0u;
	params.time = time_;
	params.burstCursor = burstCursor_;

	uint32_t spawnCount = (std::min)(static_cast<uint32_t>(pendingBursts_.size()), 8u);
	params.burstSpawnCount = spawnCount;
	uint32_t addedParticles = 0;
	for (uint32_t i = 0; i < spawnCount; ++i) {
		std::memcpy(&params.bursts[i], &pendingBursts_[i], sizeof(BurstSpawnGPU));
		addedParticles += pendingBursts_[i].count;
	}
	burstCursor_ = (burstCursor_ + addedParticles) % kTotalParticles;
	pendingBursts_.clear();
	reset_ = false;

	ParticleCSConstant* mappedCS = nullptr;
	csConstantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&mappedCS));
	std::memcpy(mappedCS, &params, sizeof(ParticleCSConstant));
	csConstantBuffer_->Unmap(0, nullptr);

	cl->SetComputeRootSignature(computeRootSignature_.Get());
	cl->SetPipelineState(computePipeline_.Get());
	cl->SetComputeRootConstantBufferView(0, csConstantBuffer_->GetGPUVirtualAddress());
	cl->SetComputeRootUnorderedAccessView(1, buffer->GetGPUVirtualAddress());
	cl->Dispatch((kTotalParticles + 255) / 256, 1, 1);

	// UAV(書き込み) → 頂点シェーダから読む状態へ遷移
	CD3DX12_RESOURCE_BARRIER toRead = CD3DX12_RESOURCE_BARRIER::Transition(buffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	cl->ResourceBarrier(1, &toRead);

	// ---- 2. グラフィックスシェーダ（Particle.VS.hlsl / Particle.PS.hlsl）で全粒を描画 ----
	D3D12_VIEWPORT viewport{};
	viewport.Width = static_cast<float>(dxCommon->GetBackBufferWidth());
	viewport.Height = static_cast<float>(dxCommon->GetBackBufferHeight());
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	D3D12_RECT scissorRect{0, 0, static_cast<LONG>(dxCommon->GetBackBufferWidth()), static_cast<LONG>(dxCommon->GetBackBufferHeight())};
	cl->RSSetViewports(1, &viewport);
	cl->RSSetScissorRects(1, &scissorRect);

	cl->SetGraphicsRootSignature(graphicsRootSignature_.Get());
	cl->SetPipelineState(graphicsPipeline_.Get());
	cl->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	cl->SetGraphicsRootConstantBufferView(0, materialBuffer_->GetGPUVirtualAddress());
	cl->SetGraphicsRootShaderResourceView(1, buffer->GetGPUVirtualAddress());

	for (const auto& mesh : model_->GetMeshes()) {
		// Resources/particle のモデルの画像(Material のテクスチャ)を貼る。無ければ、予備の白い画像
		Material* material = mesh->GetMaterial();
		const uint32_t textureHandle = (material && material->GetTextureHandle() != 0) ? material->GetTextureHandle() : textureHandle_;
		TextureManager::GetInstance()->SetGraphicsRootDescriptorTable(cl, 2, textureHandle);
		cl->IASetVertexBuffers(0, 1, &mesh->GetVBView());
		cl->IASetIndexBuffer(&mesh->GetIBView());
		cl->DrawIndexedInstanced(static_cast<UINT>(mesh->GetIndices().size()), kTotalParticles, 0, 0, 0);
	}

	// 次フレームのコンピュートのため UAV に戻す
	CD3DX12_RESOURCE_BARRIER toWrite = CD3DX12_RESOURCE_BARRIER::Transition(buffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	cl->ResourceBarrier(1, &toWrite);
}