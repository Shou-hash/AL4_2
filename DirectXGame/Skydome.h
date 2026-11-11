#pragma once
#include "Kamataengine.h"

class Skydome {
public:
	void Initialize(KamataEngine::Model* model, KamataEngine::Camera* camera);

	void Update();

	void Draw();

	void SetModel(KamataEngine::Model* model) { model_ = model; }

	// 天球をプレイヤーに追従させるための位置設定
	void SetPosition(const KamataEngine::Vector3& position) { worldTransform_.translation_ = position; }

private:
	KamataEngine::WorldTransform worldTransform_;

	KamataEngine::Model* model_ = nullptr;
	KamataEngine::Camera* camera_ = nullptr;
};