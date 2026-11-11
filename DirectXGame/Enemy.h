#pragma once
#include "3d/AxisIndicator.h"
#include "3d/DebugCamera.h"
#include "Collider.h"
#include "EnemyBullet.h"
#include "TimedCall.h"
#include <KamataEngine.h>
#include <functional>
#include <list>

// 自機クラスの前方宣言
class Player;
class BaseEnemyState;

/// <summary>
/// 敵クラス（Colliderを継承）
/// </summary>
class Enemy : public Collider {
public:
	static const uint32_t kFireInterval = 60;

	~Enemy();

	void Initialize(KamataEngine::Model* model, uint32_t textureHandle, Player* player, const KamataEngine::Vector3& position);
	void Update();
	void Draw(const KamataEngine::Camera& camera);
	void ChangeState(BaseEnemyState* newState);

	void InitializeApproachPhase();
	void FireAndReset();
	void ClearTimedCalls();
	void Fire();

	// Player の Setter
	void SetPlayer(Player* player) { player_ = player; }
	Player* GetPlayer() const { return player_; }

	// デスフラグ（撃破された/画面外へ離脱した）
	bool IsDead() const { return isDead_; }

	// ★ Colliderの関数をオーバーライド
	void OnCollision() override;
	KamataEngine::Vector3 GetWorldPosition() const override;

	// 敵弾リストを取得する getter（const参照渡し）
	const std::list<EnemyBullet*>& GetBullets() const { return bullets_; }

	KamataEngine::WorldTransform& GetWorldTransform() { return worldTransform_; }

private:
	KamataEngine::WorldTransform worldTransform_;
	KamataEngine::Model* model_ = nullptr;
	uint32_t textureHandle_ = 0;
	BaseEnemyState* state_ = nullptr;

	// 自機キャラのポインタ（所有権は持たない）
	Player* player_ = nullptr;

	bool isDead_ = false;

	std::list<EnemyBullet*> bullets_;
	std::list<TimedCall*> timedCalls_;
};