#pragma once
#include "BaseEnemyState.h"
#include "KamataEngine.h"

// 離脱フェーズ
class EnemyStateLeave : public BaseEnemyState {
public:
	EnemyStateLeave(Enemy* enemy, const KamataEngine::Vector3& velocity);
	void Update() override;

private:
	KamataEngine::Vector3 velocity_;
};
