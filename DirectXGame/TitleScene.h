#pragma once
#include "BackgroundBox.h"
#include "CameraController.h"
#include "KamataEngine.h"
#include "LightingManager.h"
#include "ParticleSystem.h"
#include "Player.h"
#include "Skydome.h"
#include "StageManager.h"

/// <summary>
/// タイトルシーンクラス（ゲーム画面と同じ背景・ステージを表示し、プレイヤーと敵は静止）
/// </summary>
class TitleScene {
public:
	~TitleScene();

	// 初期化
	void Initialize();

	// 更新
	void Update();

	// 描画
	void Draw();

	// ★ シーン終了フラグの取得
	bool IsFinished() const { return isFinished_; }

private:
	KamataEngine::Camera camera_;
	KamataEngine::Input* input_ = nullptr;

	// プレイヤー（静止表示用）
	Player* player_ = nullptr;
	uint32_t playerTex_ = 0;
	KamataEngine::Model* model_ = nullptr;

	// 天球
	Skydome* skydome_ = nullptr;
	KamataEngine::Model* modelSkydome_ = nullptr;

	// カメラコントローラ（2番目の視点固定）
	CameraController* cameraController_ = nullptr;

	// ライティング / ステージ / 背景 / GPUパーティクル
	LightingManager* lighting_ = nullptr;
	StageManager* stage_ = nullptr;
	BackgroundBox* background_ = nullptr;
	ParticleSystem* rain_ = nullptr;
	KamataEngine::Model* modelGalaxy_[3] = {};
	KamataEngine::Model* modelFrame_ = nullptr;
	KamataEngine::Model* modelStar_ = nullptr;
	KamataEngine::Model* modelMoon_ = nullptr;
	KamataEngine::Model* modelParticle_ = nullptr;

	// シーン終了フラグ
	bool isFinished_ = false;
};