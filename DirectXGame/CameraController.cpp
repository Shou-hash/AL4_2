#include "CameraController.h"
#include "Matrix4x4.h"
#include "Player.h"
#include <cassert>
#include <cmath>

using namespace KamataEngine;

namespace {
const float kPi = 3.14159265f;

// カメラ専用ビュー行列の計算（回転順序: Y(Yaw) -> X(Pitch) -> Z(Roll)）
Matrix4x4 MakeCameraViewMatrix(const Vector3& rot, const Vector3& translate) {
	float cx = std::cos(rot.x), sx = std::sin(rot.x);
	float cy = std::cos(rot.y), sy = std::sin(rot.y);
	float cz = std::cos(rot.z), sz = std::sin(rot.z);

	Matrix4x4 matRotX{}, matRotY{}, matRotZ{};
	matRotX.m[0][0] = 1.0f;
	matRotX.m[3][3] = 1.0f;
	matRotX.m[1][1] = cx;
	matRotX.m[1][2] = sx;
	matRotX.m[2][1] = -sx;
	matRotX.m[2][2] = cx;

	matRotY.m[1][1] = 1.0f;
	matRotY.m[3][3] = 1.0f;
	matRotY.m[0][0] = cy;
	matRotY.m[0][2] = -sy;
	matRotY.m[2][0] = sy;
	matRotY.m[2][2] = cy;

	matRotZ.m[2][2] = 1.0f;
	matRotZ.m[3][3] = 1.0f;
	matRotZ.m[0][0] = cz;
	matRotZ.m[0][1] = sz;
	matRotZ.m[1][0] = -sz;
	matRotZ.m[1][1] = cz;

	// ★ カメラ視点用に Yaw(Y) -> Pitch(X) -> Roll(Z) の順で合成してジンバルロックを防止
	Matrix4x4 matRot = Multiply(matRotY, Multiply(matRotX, matRotZ));

	Matrix4x4 world{};
	world.m[0][0] = matRot.m[0][0];
	world.m[0][1] = matRot.m[0][1];
	world.m[0][2] = matRot.m[0][2];
	world.m[0][3] = 0.0f;
	world.m[1][0] = matRot.m[1][0];
	world.m[1][1] = matRot.m[1][1];
	world.m[1][2] = matRot.m[1][2];
	world.m[1][3] = 0.0f;
	world.m[2][0] = matRot.m[2][0];
	world.m[2][1] = matRot.m[2][1];
	world.m[2][2] = matRot.m[2][2];
	world.m[2][3] = 0.0f;
	world.m[3][0] = translate.x;
	world.m[3][1] = translate.y;
	world.m[3][2] = translate.z;
	world.m[3][3] = 1.0f;

	// カメラのワールド行列の逆行列（ビュー行列）を作成
	Matrix4x4 view{};
	view.m[0][0] = world.m[0][0];
	view.m[0][1] = world.m[1][0];
	view.m[0][2] = world.m[2][0];
	view.m[0][3] = 0.0f;
	view.m[1][0] = world.m[0][1];
	view.m[1][1] = world.m[1][1];
	view.m[1][2] = world.m[2][1];
	view.m[1][3] = 0.0f;
	view.m[2][0] = world.m[0][2];
	view.m[2][1] = world.m[1][2];
	view.m[2][2] = world.m[2][2];
	view.m[2][3] = 0.0f;
	view.m[3][0] = -(world.m[3][0] * view.m[0][0] + world.m[3][1] * view.m[1][0] + world.m[3][2] * view.m[2][0]);
	view.m[3][1] = -(world.m[3][0] * view.m[0][1] + world.m[3][1] * view.m[1][1] + world.m[3][2] * view.m[2][1]);
	view.m[3][2] = -(world.m[3][0] * view.m[0][2] + world.m[3][1] * view.m[1][2] + world.m[3][2] * view.m[2][2]);
	view.m[3][3] = 1.0f;
	return view;
}
} // namespace

void CameraController::Initialize() {
	camera_.Initialize();
	// Initialize() で上書きされるので、その後に設定する
	camera_.farZ = 2000.0f;
	SetMode(CameraMode::TopDown);
	camera_.rotation_ = targetRotation_;
}

void CameraController::SetMode(CameraMode mode) {
	// 各視点の Offset 定数（スクショの値を反映）
	static const Vector3 kOffsets[] = {
	    {0.0f,    0.0f,     -30.0f  }, // TopDown    : 手前から正面
	    {8.108f,  -16.216f, -17.000f}, // ThirdPerson: スクショ1の値
	    {19.190f, -40.541f, 0.001f  }, // RightSide  : スクショ2の値
	};
	// 各視点の Rotation 定数（スクショの値を反映）
	static const Vector3 kRotations[] = {
	    {0.0f,    0.0f,    0.0f}, // TopDown    : 手前から正面
	    {-0.849f, -0.310f, 0.0f}, // ThirdPerson: スクショ1の値
	    {-1.528f, -0.552f, 0.0f}, // RightSide  : スクショ2の値
	};

	mode_ = mode;
	int index = static_cast<int>(mode);
	targetOffset_ = kOffsets[index];
	targetRotation_ = kRotations[index];
}

void CameraController::Reset() {
	assert(target_);
	Vector3 p = target_->GetWorldPosition();
	destination_ = {p.x + targetOffset_.x, p.y + targetOffset_.y, p.z + targetOffset_.z};
	camera_.translation_ = destination_;
	camera_.rotation_ = targetRotation_;

	// 自前でビュー行列を計算して転送
	camera_.matView = MakeCameraViewMatrix(camera_.rotation_, camera_.translation_);
	camera_.TransferMatrix();
}

void CameraController::Update() {
	assert(target_);

#ifdef _DEBUG

	// 視点切り替え
	Input* input = Input::GetInstance();
	if (input->TriggerKey(DIK_1)) {
		SetMode(CameraMode::TopDown);
	} else if (input->TriggerKey(DIK_2)) {
		SetMode(CameraMode::ThirdPerson);
	} else if (input->TriggerKey(DIK_3)) {
		SetMode(CameraMode::RightSide);
	}

#endif // _DEBUG

	Vector3 p = target_->GetWorldPosition();
	destination_ = {p.x + targetOffset_.x, p.y + targetOffset_.y, p.z + targetOffset_.z};

	// 目標へLerpで少しずつ近づける（座標・回転とも）
	camera_.translation_.x += (destination_.x - camera_.translation_.x) * kInterpolationRate;
	camera_.translation_.y += (destination_.y - camera_.translation_.y) * kInterpolationRate;
	camera_.translation_.z += (destination_.z - camera_.translation_.z) * kInterpolationRate;
	camera_.rotation_.x += (targetRotation_.x - camera_.rotation_.x) * kInterpolationRate;
	camera_.rotation_.y += (targetRotation_.y - camera_.rotation_.y) * kInterpolationRate;
	camera_.rotation_.z += (targetRotation_.z - camera_.rotation_.z) * kInterpolationRate;

	// ★ 正しい回転順序でビュー行列を作成して転送
	camera_.matView = MakeCameraViewMatrix(camera_.rotation_, camera_.translation_);
	camera_.TransferMatrix();

	// ImGui表示（カメラ調整用デバッグウィンドウ）
	ImGui::Begin("Camera Controller");

	// モード切り替え用ボタン
	int currentMode = static_cast<int>(mode_);
	if (ImGui::RadioButton("1: TopDown", currentMode == 0)) {
		SetMode(CameraMode::TopDown);
	}
	ImGui::SameLine();
	if (ImGui::RadioButton("2: ThirdPerson", currentMode == 1)) {
		SetMode(CameraMode::ThirdPerson);
	}
	ImGui::SameLine();
	if (ImGui::RadioButton("3: RightSide", currentMode == 2)) {
		SetMode(CameraMode::RightSide);
	}

	ImGui::Separator();

	// カメラ設定値の調整
	ImGui::SliderFloat3("Target Offset", &targetOffset_.x, -100.0f, 100.0f);
	ImGui::SliderFloat3("Target Rotation", &targetRotation_.x, -kPi * 2.0f, kPi * 2.0f);

	ImGui::Separator();

	// 現在の計算結果（座標・回転）の確認表示
	ImGui::Text("Camera Position: (%.2f, %.2f, %.2f)", camera_.translation_.x, camera_.translation_.y, camera_.translation_.z);
	ImGui::Text("Camera Rotation: (%.2f, %.2f, %.2f)", camera_.rotation_.x, camera_.rotation_.y, camera_.rotation_.z);

	if (ImGui::Button("Reset to Current Mode Default")) {
		SetMode(mode_);
	}

	ImGui::End();
}