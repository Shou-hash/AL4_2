#pragma once
#include "KamataEngine.h"
#include <memory>
#include <vector>

/// <summary>
/// 背景。PictureFrame の板4枚（奥・上・下・左）で箱状の空間を組み、
/// 内側に星(star2)を各板と同じ向きでランダム配置し、左側に月(moon)を置く。
/// 手前(-Z)と右(+X)は、カメラがあるので開けてある。箱はプレイヤーに追従する。
/// </summary>
class BackgroundBox {
public:
	void Initialize(KamataEngine::Model* frameModel, KamataEngine::Model* starModel, KamataEngine::Model* moonModel);
	void Update(const KamataEngine::Vector3& playerPos, KamataEngine::LightGroup* lightGroup = nullptr);
	// 内部で Model::PreDraw(kNone)/PostDraw を行う
	void Draw(const KamataEngine::Camera& camera);

private:
	struct Part {
		KamataEngine::WorldTransform worldTransform;
		KamataEngine::Vector3 scale{1, 1, 1};
		KamataEngine::Vector3 rotation{0, 0, 0};
		KamataEngine::Vector3 offset{0, 0, 0};  // 箱の中心からの位置
		KamataEngine::Vector3 forward{0, 0, 1}; // モデルが向いている正面方向ベクトル
	};

	void UpdatePart(Part& part, const KamataEngine::Vector3& base);

	KamataEngine::Model* frameModel_ = nullptr;
	KamataEngine::Model* starModel_ = nullptr;
	KamataEngine::Model* moonModel_ = nullptr;

	std::vector<std::unique_ptr<Part>> plates_;
	std::vector<std::unique_ptr<Part>> stars_;
	Part moon_;

	float centerY_ = 0.0f;
	bool firstUpdate_ = true;

	// ---- 調整用 ----
	static constexpr float kPlateScale = 2.5f;   // 板の拡大率
	static constexpr int kStarsPerPlate = 50;    // 板1枚あたりの星の数
	static constexpr float kStarScaleMin = 0.8f; // 星の大きさ(倍率)
	static constexpr float kStarScaleMax = 2.4f;
	static constexpr float kMoonScale = 12.0f;

	// 星のポイントライト設定
	const KamataEngine::Vector3 starLightColor_ = {1.0f, 0.55f, 0.10f}; // オレンジ色
	static constexpr float kStarLightIntensity = 4.0f;                  // 強度
	static constexpr float kStarFrontOffset = 1.2f;                     // 星の正面方向への手前移動オフセット量
};