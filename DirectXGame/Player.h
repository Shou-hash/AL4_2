#pragma once
#include "3d/DebugCamera.h"
#include "Collider.h"
#include "Kamataengine.h"
#include "PlayerBullet.h"
#include <list>

class StageManager;
class Enemy;                // ★ 前方宣言
class EnemyBullet;          // ★ 前方宣言
class EffectParticleSystem; // ★ 前方宣言

/// <summary>
/// プレイヤークラス（Colliderを継承）
/// </summary>
class Player : public Collider {
public:
	// デストラクタ
	~Player();

	// 初期化
	void Initialize(KamataEngine::Model* model, uint32_t textureHandle);

	// 更新
	void Update();

	// ★ 移動や入力を行わず行列だけ更新する（TitleScene / EndScene の静止表示用）
	void UpdateMatrix();

	// 描画
	void Draw(KamataEngine::Camera* camera);

	// ★ Colliderの純粋仮想関数・仮想関数をオーバーライド
	KamataEngine::Vector3 GetWorldPosition() const override;
	void OnCollision() override;

	// 自弾リストを取得する getter（参照渡し）
	const std::list<PlayerBullet*>& GetBullets() const { return bullets_; }

	// 足場(ステージ)の設定。自動前進・ジャンプ・着地の判定に使う
	void SetStage(const StageManager* stage) { stage_ = stage; }

	// ★ 敵リストの設定（自動照準用）
	void SetEnemies(const std::list<Enemy*>* enemies) { enemies_ = enemies; }

	// ★ 追加: 弾命中パーティクル用のエフェクトシステムを設定
	void SetEffectParticleSystem(EffectParticleSystem* effectParticles) { effectParticles_ = effectParticles; }

	// ★ 追加: 接地状態の取得（着地揺れ・裏面パーティクル判定用）
	bool IsOnGround() const { return onGround_; }

private:
	/// <summary>
	/// 攻撃
	/// </summary>
	void Attack();

private:
	KamataEngine::WorldTransform worldTransform_;
	KamataEngine::Model* model_ = nullptr;
	uint32_t textureHandle_ = 0u;

	// キーボード入力
	KamataEngine::Input* input_ = nullptr;

	// 弾のリスト（複数管理）
	std::list<PlayerBullet*> bullets_;

	const StageManager* stage_ = nullptr;

	// ★ 敵リストへの参照（所有権なし）
	const std::list<Enemy*>* enemies_ = nullptr;

	// ★ 追加: 弾命中パーティクル用のエフェクトシステムへの参照
	EffectParticleSystem* effectParticles_ = nullptr;

	// ★ 縦方向（Z軸）の速度とジャンプ状態
	float velocityZ_ = 0.0f;
	bool onGround_ = false;
};