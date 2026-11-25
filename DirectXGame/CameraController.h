#pragma once
#include "KamataEngine.h"

class Player;

// 視点モード（キー 1/2/3 で切り替え）
enum class CameraMode {
	TopDown,     // 1: 見下ろし（現状のまま）
	ThirdPerson, // 2: 第三人称（少し見下ろし＋少し右斜め）
	RightSide,   // 3: 自キャラの右側
};

/// <summary>
/// 自キャラ追従カメラ（視点切り替え対応）
/// </summary>
class CameraController {
public:
	void Initialize();
	// 一瞬で追従位置へ合わせる
	void Reset();
	// 毎フレーム追従して行列を更新
	void Update();

	void SetTarget(Player* target) { target_ = target; }
	void SetMode(CameraMode mode);
	CameraMode GetMode() const { return mode_; }
	const KamataEngine::Camera& GetCamera() const { return camera_; }

	// ★ 追加: 現在のターゲットオフセットを取得
	const KamataEngine::Vector3& GetTargetOffset() const { return targetOffset_; }

private:
	KamataEngine::Camera camera_;
	Player* target_ = nullptr;

	CameraMode mode_ = CameraMode::TopDown;
	// 自キャラからの相対座標と、自キャラを見据える回転角（モードごとに SetMode で決まる）
	KamataEngine::Vector3 targetOffset_{0.0f, 0.0f, -30.0f};
	KamataEngine::Vector3 targetRotation_{0.0f, 0.0f, 0.0f};
	KamataEngine::Vector3 destination_{};
	static inline const float kInterpolationRate = 0.1f;
};