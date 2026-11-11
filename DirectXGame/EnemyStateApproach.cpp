#include "EnemyStateApproach.h"
#include "Enemy.h"
#include "EnemyStateLeave.h"
#include "Player.h"
#include <cmath>

// コンストラクタ
EnemyStateApproach::EnemyStateApproach(Enemy* enemy) : BaseEnemyState("State Approach", enemy) {}

void EnemyStateApproach::Update() {
	DebugLog(); // ログ表示

	if (enemy_ == nullptr) {
		return;
	}

	KamataEngine::WorldTransform& transform = enemy_->GetWorldTransform();
	KamataEngine::Vector3 playerPos = enemy_->GetPlayer()->GetWorldPosition();

	// 自機へ向かうXY平面上の方向
	float dx = playerPos.x - transform.translation_.x;
	float dy = playerPos.y - transform.translation_.y;
	float dist = std::sqrt(dx * dx + dy * dy);

	// 近づいたら、その向きのまま通り抜ける「離脱ステート」へ
	const float kLeaveDistance = 10.0f;
	if (dist < kLeaveDistance) {
		const float kLeaveSpeed = 0.2f;
		KamataEngine::Vector3 velocity = {dx / dist * kLeaveSpeed, dy / dist * kLeaveSpeed, 0.0f};
		enemy_->ChangeState(new EnemyStateLeave(enemy_, velocity)); // this は破棄されるので以降触らない
		return;
	}

	const float kSpeed = 0.1f;
	transform.translation_.x += dx / dist * kSpeed;
	transform.translation_.y += dy / dist * kSpeed;
}
