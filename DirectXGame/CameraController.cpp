#include "CameraController.h"
#include "Player.h"
#include <cassert>
#include <cmath>

using namespace KamataEngine;

namespace {
const float kPi = 3.14159265f;
}

void CameraController::Initialize() {
	camera_.Initialize();
	// Initialize() で上書きされるので、その後に設定する
	camera_.farZ = 2000.0f;
	SetMode(CameraMode::TopDown);
	camera_.rotation_ = targetRotation_;
}

void CameraController::SetMode(CameraMode mode) {
	static const Vector3 kOffsets[] = {
	    {0.0f,  0.0f, -30.0f}, // TopDown    : 手前から正面
	    {8.0f,  6.0f, -25.0f}, // ThirdPerson: 少し右・少し上・手前
	    {30.0f, 0.0f, 0.0f  }, // RightSide  : 右(+X)から自キャラを見る
	};
	mode_ = mode;
	targetOffset_ = kOffsets[static_cast<int>(mode)];

	// オフセットの逆向き(自キャラ方向)から、見下ろし角(x)と向き(y)を求める
	float dx = -targetOffset_.x;
	float dy = -targetOffset_.y;
	float dz = -targetOffset_.z;

	// ★ Z軸に90度 (kPi * 0.5f) を設定
	targetRotation_ = {
	    std::atan2(-dy, std::sqrt(dx * dx + dz * dz)), std::atan2(dx, dz),
	    kPi * 0.5f // ← Z軸90度回転
	};
}

void CameraController::Reset() {
	assert(target_);
	Vector3 p = target_->GetWorldPosition();
	destination_ = {p.x + targetOffset_.x, p.y + targetOffset_.y, p.z + targetOffset_.z};
	camera_.translation_ = destination_;
	camera_.rotation_ = targetRotation_;
	camera_.UpdateMatrix();
}

void CameraController::Update() {
	assert(target_);

	// 視点切り替え
	Input* input = Input::GetInstance();
	if (input->TriggerKey(DIK_1)) {
		SetMode(CameraMode::TopDown);
	} else if (input->TriggerKey(DIK_2)) {
		SetMode(CameraMode::ThirdPerson);
	} else if (input->TriggerKey(DIK_3)) {
		SetMode(CameraMode::RightSide);
	}

	Vector3 p = target_->GetWorldPosition();
	destination_ = {p.x + targetOffset_.x, p.y + targetOffset_.y, p.z + targetOffset_.z};

	// 目標へLerpで少しずつ近づける（座標・回転とも）
	camera_.translation_.x += (destination_.x - camera_.translation_.x) * kInterpolationRate;
	camera_.translation_.y += (destination_.y - camera_.translation_.y) * kInterpolationRate;
	camera_.translation_.z += (destination_.z - camera_.translation_.z) * kInterpolationRate;
	camera_.rotation_.x += (targetRotation_.x - camera_.rotation_.x) * kInterpolationRate;
	camera_.rotation_.y += (targetRotation_.y - camera_.rotation_.y) * kInterpolationRate;
	camera_.rotation_.z += (targetRotation_.z - camera_.rotation_.z) * kInterpolationRate;

	camera_.UpdateMatrix();
}
