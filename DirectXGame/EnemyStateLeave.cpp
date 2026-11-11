#include "EnemyStateLeave.h"
#include "Enemy.h"

EnemyStateLeave::EnemyStateLeave(Enemy* enemy, const KamataEngine::Vector3& velocity) : BaseEnemyState("State Leave", enemy), velocity_(velocity) {
	if (enemy_) {
		// 離脱フェーズに入ったらタイマー（自動発射）を停止・クリアする
		enemy_->ClearTimedCalls();
	}
}

void EnemyStateLeave::Update() {
	DebugLog(); // ログ表示

	if (enemy_ == nullptr) {
		return;
	}

	// 接近時の向きのまま通り抜ける
	KamataEngine::WorldTransform& transform = enemy_->GetWorldTransform();
	transform.translation_.x += velocity_.x;
	transform.translation_.y += velocity_.y;
	transform.translation_.z += velocity_.z;
}
