#pragma once
#include "3d/DebugCamera.h"
#include "BackgroundBox.h"
#include "CameraController.h" // RailCameraController.h を置き換え
#include "Collider.h"
#include "CollisionManager.h"
#include "EffectParticleSystem.h" // ★ 追加
#include "Enemy.h"
#include "KamataEngine.h"
#include "LightingManager.h"
#include "ParticleSystem.h"
#include "Player.h"
#include "Skydome.h"
#include "StageManager.h"
#include <list>

class GameScene {
public:
	~GameScene();

	// 初期化
	void Initialize();

	// 更新
	void Update();

	// 描画
	void Draw();

	/// <summary>
	/// 衝突判定と応答
	/// </summary>
	void CheckAllCollisions();

	// ★ シーン終了フラグの取得
	bool IsFinished() const { return isFinished_; }

private:
	/// <summary>
	/// コライダーペア間の衝突判定と応答
	/// </summary>
	void CheckCollisionPair(Collider* colliderA, Collider* colliderB);

private:
	// 敵（全方位から次々出現する）
	std::list<Enemy*> enemies_;
	int32_t spawnTimer_ = 0;
	static inline const int32_t kSpawnInterval = 120;
	KamataEngine::Model* enemyModel_ = nullptr;
	uint32_t enemyTextureHandle_ = 0;

	KamataEngine::Camera camera_;

	Player* player_ = nullptr;
	uint32_t playerTex_ = 0;
	KamataEngine::Model* model_ = nullptr;

	KamataEngine::DebugCamera* debugCamera_ = nullptr;
	bool isDebugCameraActive_ = false;

	KamataEngine::Input* input_ = nullptr;

	// 衝突マネージャ
	CollisionManager* collisionManager_ = nullptr;

	// 天球
	Skydome* skydome_ = nullptr;
	// 3Dモデル
	KamataEngine::Model* modelSkydome_ = nullptr;

	// レールカメラ
	CameraController* cameraController_ = nullptr;

	// ライティング / ステージ / 背景 / GPUパーティクル
	LightingManager* lighting_ = nullptr;
	StageManager* stage_ = nullptr;
	BackgroundBox* background_ = nullptr;
	ParticleSystem* rain_ = nullptr;
	EffectParticleSystem* effectParticles_ = nullptr;
	KamataEngine::Model* modelGalaxy_[3] = {};
	KamataEngine::Model* modelFrame_ = nullptr;
	KamataEngine::Model* modelStar_ = nullptr;
	KamataEngine::Model* modelMoon_ = nullptr;
	KamataEngine::Model* modelParticle_ = nullptr;

	// ★ 追加: プレイヤーの着地検知用フラグ
	bool wasPlayerOnGround_ = false;

	// シーン終了フラグ
	bool isFinished_ = false;
};