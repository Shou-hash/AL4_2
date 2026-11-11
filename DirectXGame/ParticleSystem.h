#pragma once
#include "KamataEngine.h"
#include <d3d12.h>
#include <wrl.h>

/// <summary>
/// GPUパーティクル（雨）。位置の更新はコンピュートシェーダ、描画はインスタンシング。
/// CPUは毎フレーム Dispatch と Draw を積むだけで、粒ごとの処理はしない。
/// </summary>
class ParticleSystem {
public:
	static const uint32_t kParticleCount = 8192;

	// model: 粒の形状とテクスチャ(particle: circle.png の板ポリ)
	void Initialize(KamataEngine::Model* model);

	// center: 雨を降らせる中心（プレイヤー座標）。
	// Model::PostDraw() の後に呼ぶこと（独自のパイプラインを使うため）
	void Draw(const KamataEngine::Camera& camera, const KamataEngine::Vector3& center);

private:
	KamataEngine::Model* model_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> particleBuffer_;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> computeRootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> computePipeline_;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> graphicsRootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipeline_;

	uint32_t frame_ = 0;
	bool reset_ = true; // 初回のみ、全粒をランダム配置する
};
