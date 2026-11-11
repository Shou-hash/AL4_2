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
};
