#pragma once
#include "KamataEngine.h"

/// <summary>
/// ライティングマネージャ（夜の雰囲気）
/// </summary>
class LightingManager {
public:
	~LightingManager();

	void Initialize();
	// 毎フレーム：プレイヤーに追従する光を更新して転送
	void Update(const KamataEngine::Vector3& playerPos);

	// モデルにこのライトを適用する（描画前に1回でよい）
	void Apply(KamataEngine::Model* model) { model->SetLightGroup(lightGroup_); }

	// 月の位置（画面の左・上・奥）。月モデルの配置にも使う
	static KamataEngine::Vector3 GetMoonPosition(const KamataEngine::Vector3& playerPos);

	KamataEngine::LightGroup* GetLightGroup() const { return lightGroup_; }

private:
	KamataEngine::LightGroup* lightGroup_ = nullptr;

	// 環境光：夜の深みのある青
	KamataEngine::Vector3 ambientColor_ = {0.12f, 0.15f, 0.35f};

	// 平行光源0：月明かりの方向光（青白）
	bool dirLight0Active_ = true;
	KamataEngine::Vector3 dirLight0Dir_ = {-1.0f, 0.8f, -0.6f};
	KamataEngine::Vector3 dirLight0Color_ = {0.40f, 0.55f, 0.95f};

	// 平行光源1：輪郭強調（リムライト）
	bool dirLight1Active_ = true;
	KamataEngine::Vector3 dirLight1Dir_ = {0.3f, 0.2f, 1.0f};
	KamataEngine::Vector3 dirLight1Color_ = {0.20f, 0.18f, 0.40f};

	// 点光源0：プレイヤーのランタン（暖色）
	bool pointLight0Active_ = true;
	KamataEngine::Vector3 pointLight0Color_ = {1.0f, 0.70f, 0.40f};
	float pointLight0Intensity_ = 2.5f;
	float pointLight0Radius_ = 12.0f;
	float pointLight0Decay_ = 1.5f;
	KamataEngine::Vector3 lanternOffset_ = {0.0f, 3.0f, -6.0f};

	// 点光源1：月のポイントライト
	bool pointLight1Active_ = true;
	KamataEngine::Vector3 pointLight1Color_ = {0.60f, 0.75f, 1.0f};
	float pointLight1Intensity_ = 6.0f;
	float pointLight1Radius_ = 35.0f;
	float pointLight1Decay_ = 1.5f;
	float pointLight1ZOffset_ = -5.0f;
	static KamataEngine::Vector3 moonOffset_;
};