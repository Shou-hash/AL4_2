#include "LightingManager.h"
#include <cmath>

using namespace KamataEngine;

// static メンバ変数の実体定義（月のオフセット座標）
Vector3 LightingManager::moonOffset_ = {-30.0f, 30.0f, -25.0f};

namespace {
Vector3 Normalized(const Vector3& v) {
	float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
	if (len == 0.0f)
		return {0.0f, 0.0f, 0.0f};
	return {v.x / len, v.y / len, v.z / len};
}
} // namespace

LightingManager::~LightingManager() { delete lightGroup_; }

void LightingManager::Initialize() {
	lightGroup_ = LightGroup::Create();

	// 環境光：夜の深みのある青
	lightGroup_->SetAmbientColor(ambientColor_);

	// 平行光源0：月明かりの方向光（青白）
	lightGroup_->SetDirLightActive(0, dirLight0Active_);
	lightGroup_->SetDirLightDir(0, Normalized(dirLight0Dir_));
	lightGroup_->SetDirLightColor(0, dirLight0Color_);

	// 平行光源1：輪郭強調（リムライト）
	lightGroup_->SetDirLightActive(1, dirLight1Active_);
	lightGroup_->SetDirLightDir(1, Normalized(dirLight1Dir_));
	lightGroup_->SetDirLightColor(1, dirLight1Color_);

	lightGroup_->SetDirLightActive(2, false);

	// 点光源0：プレイヤーのランタン（暖色）
	lightGroup_->SetPointLightActive(0, pointLight0Active_);
	lightGroup_->SetPointLightColor(0, pointLight0Color_);
	lightGroup_->SetPointLightIntensity(0, pointLight0Intensity_);
	lightGroup_->SetPointLightRadius(0, pointLight0Radius_);
	lightGroup_->SetPointLightDecay(0, pointLight0Decay_);

	// 点光源1：月のポイントライト
	lightGroup_->SetPointLightActive(1, pointLight1Active_);
	lightGroup_->SetPointLightColor(1, pointLight1Color_);
	lightGroup_->SetPointLightIntensity(1, pointLight1Intensity_);
	lightGroup_->SetPointLightRadius(1, pointLight1Radius_);
	lightGroup_->SetPointLightDecay(1, pointLight1Decay_);

	// スポットライト・丸影は使わない
	for (int i = 0; i < LightGroup::kSpotLightNum; ++i) {
		lightGroup_->SetSpotLightActive(i, false);
	}
	for (int i = 0; i < LightGroup::kCircleShadowNum; ++i) {
		lightGroup_->SetCircleShadowActive(i, false);
	}

	lightGroup_->Update();
}

// 月の座標: X(左右)・Y(前進方向)は、プレイヤーに追従する。Z(落下方向)は、プレイヤーに追従せず固定にする
// (プレイヤー・カメラが落ちても、月は、一緒に落ちない。背景の箱の中心 Z = 0 と同じ基準)
Vector3 LightingManager::GetMoonPosition(const Vector3& playerPos) { return {playerPos.x + moonOffset_.x, playerPos.y + moonOffset_.y, moonOffset_.z}; }

void LightingManager::Update(const Vector3& playerPos) {
	// 環境光
	lightGroup_->SetAmbientColor(ambientColor_);

	// 平行光源0
	lightGroup_->SetDirLightActive(0, dirLight0Active_);
	lightGroup_->SetDirLightDir(0, Normalized(dirLight0Dir_));
	lightGroup_->SetDirLightColor(0, dirLight0Color_);

	// 平行光源1
	lightGroup_->SetDirLightActive(1, dirLight1Active_);
	lightGroup_->SetDirLightDir(1, Normalized(dirLight1Dir_));
	lightGroup_->SetDirLightColor(1, dirLight1Color_);

	// 点光源0（ランタン：プレイヤーの少し上・手前）
	lightGroup_->SetPointLightActive(0, pointLight0Active_);
	lightGroup_->SetPointLightColor(0, pointLight0Color_);
	lightGroup_->SetPointLightIntensity(0, pointLight0Intensity_);
	lightGroup_->SetPointLightRadius(0, pointLight0Radius_);
	lightGroup_->SetPointLightDecay(0, pointLight0Decay_);
	lightGroup_->SetPointLightPos(0, {playerPos.x + lanternOffset_.x, playerPos.y + lanternOffset_.y, playerPos.z + lanternOffset_.z});

	// 点光源1（月のポイントライト：月モデルより手前にずらした位置）
	lightGroup_->SetPointLightActive(1, pointLight1Active_);
	lightGroup_->SetPointLightColor(1, pointLight1Color_);
	lightGroup_->SetPointLightIntensity(1, pointLight1Intensity_);
	lightGroup_->SetPointLightRadius(1, pointLight1Radius_);
	lightGroup_->SetPointLightDecay(1, pointLight1Decay_);
	Vector3 moonPos = GetMoonPosition(playerPos);
	moonPos.z += pointLight1ZOffset_;
	lightGroup_->SetPointLightPos(1, moonPos);

	lightGroup_->Update();
}