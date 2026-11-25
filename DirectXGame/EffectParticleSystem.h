#pragma once
#include "KamataEngine.h"
#include <d3d12.h>
#include <vector>
#include <wrl.h>

/// <summary>
/// 外部シェーダファイル（Particle.CS.hlsl / Particle.VS.hlsl / Particle.PS.hlsl）を使った
/// 3D空間エフェクトパーティクル（ステージ裏面着地・弾命中ヒット）。
/// </summary>
class EffectParticleSystem {
public:
	static const uint32_t kTotalParticles = 4096;

	// model: 粒の形状（Resources/particle の板ポリメッシュ）
	void Initialize(KamataEngine::Model* model);

	// ImGui の操作パネルを表示し、ボタン押下やチェック状態でパーティクルを発生させる
	// cameraOffset: CameraController の targetOffset_
	void UpdateImGui(const KamataEngine::Vector3& playerPos, const KamataEngine::Vector3& cameraOffset);

	// ステージ到着時の裏面パーティクルを発生させる
	// count: 粒の数(-1 なら、ImGui の Burst Count)・directionZ: 飛ぶ向きの Z(+1 = 地面側・-1 = 空側。カメラは空側にある)
	void EmitStageBack(const KamataEngine::Vector3& backPos, int count = -1, float directionZ = 1.0f);

	// プレイヤー弾の命中時パーティクルを発生させる(count: -1 なら、ImGui の Burst Count)
	void EmitBulletHit(const KamataEngine::Vector3& hitPos, int count = -1);

	// center: エミッタ基準座標（プレイヤー座標）
	// Model::PostDraw() の後に呼ぶこと（独自のパイプラインを使うため）
	void Draw(const KamataEngine::Camera& camera, const KamataEngine::Vector3& center);

	// ★ 追加: カメラオフセット連動チェックがONかどうか
	bool IsUseCameraOffsetPos() const { return useCameraOffsetPos_; }
	// ★ 追加: 現在の確認用パーティクル発生座標を取得
	KamataEngine::Vector3 GetDebugSpawnPos() const { return lastSpawnPos_; }

private:
	struct BurstSpawn {
		float position[3];
		uint32_t count;
		float direction[3];
		float spread;
		float color[4];
		float speedMin;
		float speedMax;
		float scaleMin;
		float scaleMax;
		float lifeTime;
		float gravity;
		float pad[2];
	};

	KamataEngine::Model* model_ = nullptr;
	uint32_t textureHandle_ = 0u;

	Microsoft::WRL::ComPtr<ID3D12Resource> particleBuffer_;
	Microsoft::WRL::ComPtr<ID3D12Resource> csConstantBuffer_; // 816byte定数バッファ（CBV用）
	Microsoft::WRL::ComPtr<ID3D12Resource> materialBuffer_;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> computeRootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> computePipeline_;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> graphicsRootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipeline_;

	std::vector<BurstSpawn> pendingBursts_;
	uint32_t burstCursor_ = 0;
	uint32_t frame_ = 0;
	float time_ = 0.0f;
	bool reset_ = true; // 初回のみ全粒をリセットする

	// ImGui デバッグ調整用パラメータ
	bool useCameraOffsetPos_ = false;
	float cameraOffsetDepth_ = 10.0f;
	float debugOffset_[3] = {0.0f, 0.0f, 0.0f};
	int debugBurstCount_ = 24;    // 128 → 24 に削減
	float debugScaleMax_ = 0.45f; // 1.50 → 0.45 に縮小
	float debugSpeedMax_ = 9.0f;  // 12.0 → 9.0 に調整
	float debugLifeTime_ = 0.45f; // 0.85 → 0.45 秒に短縮
	float debugAlpha_ = 0.45f;    // ★ 追加: 眩しさ調整用アルファ倍率
	bool debugAutoBurst_ = false;
	float debugAutoTimer_ = 0.0f;
	uint32_t totalEmittedCount_ = 0;
	KamataEngine::Vector3 lastSpawnPos_{0.0f, 0.0f, 0.0f};
};