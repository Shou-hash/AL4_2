#include "Player.h"
#include "CollisionConfig.h" // ★ 衝突設定のインクルードを追加
#include "Matrix4x4.h"
#include "StageManager.h"
#include <algorithm>
#include <cassert>
#include <cmath>

using namespace KamataEngine;

// ベクトル変換 (TransformNormal)
Vector3 TransformNormal(const Vector3& v, const Matrix4x4& m) {
	Vector3 result{v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0], v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1], v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2]};
	return result;
}

Player::~Player() {
	for (PlayerBullet* bullet : bullets_) {
		delete bullet;
	}
	bullets_.clear();
}

void Player::Initialize(KamataEngine::Model* model, uint32_t textureHandle) {
	assert(model);
	model_ = model;
	textureHandle_ = textureHandle;

	worldTransform_.Initialize();
	input_ = Input::GetInstance();

	// 開始位置：空中から落ちて最初の足場に着地する
	worldTransform_.translation_ = {0.0f, 0.0f, 10.0f};

	// ★ 当たり判定の半径を設定
	SetRadius(1.0f);

	// ★ 衝突属性とマスクを設定（自分はプレイヤー、相手は自分以外）
	SetCollisionAttribute(kCollisionAttributePlayer);
	SetCollisionMask(~kCollisionAttributePlayer);
}

void Player::Update() {
	// デスフラグの立った弾を削除
	bullets_.remove_if([](PlayerBullet* bullet) {
		if (bullet->IsDead()) {
			delete bullet;
			return true;
		}
		return false;
	});

	// キャラクター旋回処理
	const float kRotSpeed = 0.02f;
	if (input_->PushKey(DIK_A)) {
		worldTransform_.rotation_.z += kRotSpeed;
	} else if (input_->PushKey(DIK_D)) {
		worldTransform_.rotation_.z -= kRotSpeed;
	}

	// ---- 自動前進＋ジャンプ（足場の上を走る）----
	const float kRunSpeed = 0.25f;     // 自動前進(+Y)の速さ
	const float kJumpPower = 0.45f;    // ジャンプの初速(Z軸)
	const float kGravity = 0.02f;      // 重力(Z軸)
	const float kStepTolerance = 1.0f; // 地形に沿って登れる/降りられる段差
	const float kFallLimitZ = -30.0f;  // これより下に落ちたら復帰

	worldTransform_.translation_.y += kRunSpeed;

	// 足元の地面の高さ(Z座標)
	float groundZ = 0.0f;
	bool hasGround = stage_ && stage_->TryGetGroundZ(worldTransform_.translation_.y, groundZ);
	float standZ = groundZ + StageManager::kPlayerHalfHeight;
	float prevZ = worldTransform_.translation_.z;

	if (onGround_) {
		if (hasGround && std::fabs(standZ - prevZ) <= kStepTolerance) {
			// 地形に沿って走る
			worldTransform_.translation_.z = standZ;
			velocityZ_ = 0.0f;
			// ジャンプ（↑キー）
			if (input_->TriggerKey(DIK_UP)) {
				velocityZ_ = kJumpPower;
				onGround_ = false;
			}
		} else {
			// 足場の端を越えた/段差が大きい → 落下開始
			onGround_ = false;
		}
	}

	if (!onGround_) {
		velocityZ_ -= kGravity;
		worldTransform_.translation_.z += velocityZ_;

		// 着地：落下中に地面を越えたら着地（少しの食い込みは許容）
		if (hasGround && velocityZ_ <= 0.0f && prevZ >= standZ - kStepTolerance && worldTransform_.translation_.z <= standZ) {
			worldTransform_.translation_.z = standZ;
			velocityZ_ = 0.0f;
			onGround_ = true;
		}
	}

	// 隙間に落ちたら、直前の足場の先頭へ戻る
	if (stage_ && worldTransform_.translation_.z < kFallLimitZ) {
		worldTransform_.translation_ = stage_->GetRespawnPosition(worldTransform_.translation_.y);
		velocityZ_ = 0.0f;
		onGround_ = false;
	}

	// 行列の再計算
	worldTransform_.matWorld_ = MakeAffineMatrix(worldTransform_.scale_, worldTransform_.rotation_, worldTransform_.translation_);
	worldTransform_.TransferMatrix();

	// 攻撃処理
	Attack();

	// 弾更新（すべての弾を更新）
	for (PlayerBullet* bullet : bullets_) {
		bullet->Update();
	}

	// ImGui表示
	ImGui::Begin("Player");
	ImGui::SliderFloat3("Position", &worldTransform_.translation_.x, -15.0f, 15.0f);
	ImGui::End();
}

void Player::Attack() {
	if (input_->TriggerKey(DIK_SPACE)) {
		// 弾の速度
		const float kBulletSpeed = 1.0f;
		Vector3 velocity(0.0f, kBulletSpeed, 0.0f); // 自機の正面は+Y

		// 速度ベクトルを自機の向きに合わせて回転させる
		velocity = TransformNormal(velocity, worldTransform_.matWorld_);

		// 弾を生成し、初期化
		PlayerBullet* newBullet = new PlayerBullet();
		newBullet->Initialize(model_, worldTransform_.translation_, velocity);

		// ★ プレイヤーの弾にも衝突属性とマスクを設定（プレイヤー陣営として扱う）
		newBullet->SetCollisionAttribute(kCollisionAttributePlayer);
		newBullet->SetCollisionMask(~kCollisionAttributePlayer);

		// 弾を登録
		bullets_.push_back(newBullet);
	}
}

void Player::OnCollision() {
	// 当たっても何もしない（仕様通り）
}

void Player::Draw(KamataEngine::Camera* camera) {
	Model::PreDraw();
	model_->Draw(worldTransform_, *camera, textureHandle_);

	// 弾描画
	for (PlayerBullet* bullet : bullets_) {
		bullet->Draw(*camera);
	}
	Model::PostDraw();
}

KamataEngine::Vector3 Player::GetWorldPosition() const {
	// ワールド行列から平行移動成分を取り出す
	KamataEngine::Vector3 worldPos;
	worldPos.x = worldTransform_.matWorld_.m[3][0];
	worldPos.y = worldTransform_.matWorld_.m[3][1];
	worldPos.z = worldTransform_.matWorld_.m[3][2];

	return worldPos;
}