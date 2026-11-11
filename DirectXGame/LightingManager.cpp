#include "LightingManager.h"
#include <cmath>

using namespace KamataEngine;

namespace {
Vector3 Normalized(const Vector3& v) {
	float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
	return {v.x / len, v.y / len, v.z / len};
}
} // namespace

LightingManager::~LightingManager() { delete lightGroup_; }

void LightingManager::Initialize() {
	lightGroup_ = LightGroup::Create();

	// 環境光：暗い青（夜）
	lightGroup_->SetAmbientColor({0.15f, 0.18f, 0.40f});

	// 平行光源0：月明かり（青白）。方向は「ライトへ向かう向き」＝左(-X)・上・手前寄り
	lightGroup_->SetDirLightActive(0, true);
	lightGroup_->SetDirLightDir(0, Normalized({-1.0f, 0.5f, -0.4f}));
	lightGroup_->SetDirLightColor(0, {0.30f, 0.40f, 0.80f});

	// 平行光源1：奥からの弱いリムライト（輪郭を浮かせる）
	lightGroup_->SetDirLightActive(1, true);
	lightGroup_->SetDirLightDir(1, Normalized({0.3f, 0.2f, 1.0f}));
	lightGroup_->SetDirLightColor(1, {0.15f, 0.12f, 0.30f});

	lightGroup_->SetDirLightActive(2, false);

	// 点光源0：プレイヤーのランタン（暖色）。足元の足場に鏡面反射が乗る
	lightGroup_->SetPointLightActive(0, true);
	lightGroup_->SetPointLightColor(0, {1.0f, 0.70f, 0.40f});
	lightGroup_->SetPointLightIntensity(0, 1.5f);
	lightGroup_->SetPointLightRadius(0, 18.0f);
	lightGroup_->SetPointLightDecay(0, 2.0f);

	// 点光源1：月の光（広範囲・青白）
	lightGroup_->SetPointLightActive(1, true);
	lightGroup_->SetPointLightColor(1, {0.50f, 0.60f, 1.0f});
	lightGroup_->SetPointLightIntensity(1, 1.2f);
	lightGroup_->SetPointLightRadius(1, 120.0f);
	lightGroup_->SetPointLightDecay(1, 1.5f);

	lightGroup_->SetPointLightActive(2, false);

	// スポットライト・丸影は使わない
	for (int i = 0; i < LightGroup::kSpotLightNum; ++i) {
		lightGroup_->SetSpotLightActive(i, false);
	}
	for (int i = 0; i < LightGroup::kCircleShadowNum; ++i) {
		lightGroup_->SetCircleShadowActive(i, false);
	}

	lightGroup_->Update();
}

Vector3 LightingManager::GetMoonPosition(const Vector3& playerPos) { return {playerPos.x - 35.0f, playerPos.y + 20.0f, 36.0f}; }

void LightingManager::Update(const Vector3& playerPos) {
	// ランタン：プレイヤーの少し上・手前(カメラ側)
	lightGroup_->SetPointLightPos(0, {playerPos.x, playerPos.y + 3.0f, playerPos.z - 6.0f});
	// 月の光
	lightGroup_->SetPointLightPos(1, GetMoonPosition(playerPos));

	lightGroup_->Update();
}
