#include "GameScene.h"
#include "3d/AxisIndicator.h"
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <string>

using namespace KamataEngine;

GameScene::~GameScene() {
	delete model_;
	delete player_;
	delete debugCamera_;

	for (Enemy* enemy : enemies_) {
		delete enemy;
	}
	delete enemyModel_;

	// ★ 衝突マネージャの解放を忘れずに実行
	delete collisionManager_;

	// 天球の解放
	delete skydome_;
	delete modelSkydome_;

	// デストラクタ
	delete cameraController_;

	delete rain_;
	delete background_;
	delete stage_;
	delete lighting_;
	delete modelParticle_;
	delete modelFrame_;
	delete modelStar_;
	delete modelMoon_;
	for (Model* model : modelGalaxy_) {
		delete model;
	}
}

void GameScene::Initialize() {
	input_ = Input::GetInstance();
	camera_.Initialize();

	// 最初の通常の視角を上から見下ろす角度（X軸90度回転 = 1.57079f）に設定
	camera_.rotation_ = {0.0f, 0.0f, 0.0f};
	// 上から見下ろしたときに見えやすいよう初期位置の高さ(Y)と奥行き(Z)を少し調整
	camera_.translation_ = {0.0f, 0.0f, -30.0f};

	debugCamera_ = new KamataEngine::DebugCamera(WinApp::kWindowWidth, WinApp::kWindowHeight);

	AxisIndicator::GetInstance()->SetVisible(true);
	AxisIndicator::GetInstance()->SetTargetCamera(&camera_);

	playerTex_ = TextureManager::Load("uvChecker.png");
	model_ = Model::Create();

	player_ = new Player();
	player_->Initialize(model_, playerTex_);

	enemyTextureHandle_ = TextureManager::Load("cube.jpg");
	enemyModel_ = Model::Create();

	srand(static_cast<unsigned>(time(nullptr))); // スポーン角度用

	// ★ 衝突マネージャの生成
	collisionManager_ = new CollisionManager();

	// 3Dモデルの生成
	modelSkydome_ = Model::CreateFromOBJ("skydome", true);
	// 天球の生成と初期化
	skydome_ = new Skydome();
	skydome_->Initialize(modelSkydome_, &camera_);

	// レールカメラの生成と初期化
	// Initialize の末尾(railCamera_ の生成部分を置き換え)
	cameraController_ = new CameraController();
	cameraController_->Initialize();
	cameraController_->SetTarget(player_);
	cameraController_->Reset();

	// ライティング（夜）。天球以外の全モデルに適用（月は自ら光る見た目にするため適用しない）
	lighting_ = new LightingManager();
	lighting_->Initialize();
	lighting_->Apply(model_);
	lighting_->Apply(enemyModel_);

	// ステージ（galaxy1〜3）。隙間を空けて並べ、プレイヤーが自動で走ってジャンプで越える
	for (int i = 0; i < 3; ++i) {
		modelGalaxy_[i] = Model::CreateFromOBJ("galaxy" + std::to_string(i + 1));
		lighting_->Apply(modelGalaxy_[i]);
	}
	stage_ = new StageManager();
	stage_->Initialize({modelGalaxy_[0], modelGalaxy_[1], modelGalaxy_[2]});
	player_->SetStage(stage_);

	// ★ プレイヤーに自動ロックオン用の敵リストを設定
	player_->SetEnemies(&enemies_);

	// 背景（板4枚の箱＋星＋月）
	modelFrame_ = Model::CreateFromOBJ("PictureFrame");
	modelStar_ = Model::CreateFromOBJ("star2");
	modelMoon_ = Model::CreateFromOBJ("moon");
	lighting_->Apply(modelFrame_);
	lighting_->Apply(modelStar_);
	background_ = new BackgroundBox();
	background_->Initialize(modelFrame_, modelStar_, modelMoon_);

	// GPUパーティクル（雨）
	modelParticle_ = Model::CreateFromOBJ("particle");
	rain_ = new ParticleSystem();
	rain_->Initialize(modelParticle_);
}

void GameScene::Update() {
#ifdef _DEBUG
	if (input_->TriggerKey(DIK_P)) {
		isDebugCameraActive_ = !isDebugCameraActive_;
	}
#endif

	// レールカメラの更新
	cameraController_->Update(); // railCamera_->Update(); を置き換え

	// マウスホイールによる遠近変更処理（ホイールの回転量を反映）
	int32_t wheel = input_->GetWheel();
	if (wheel != 0) {
		// ホイールの回転方向に応じて距離を増減（感度は 0.01f で調整）
		camera_.translation_.z += wheel * 0.01f;
		camera_.translation_.y -= wheel * 0.01f; // 見下ろし視点時に近づくよう調整
	}

	if (isDebugCameraActive_) {
		// マウス左長押し中にデバッグカメラの更新処理（移動・回転等）を行う
		if (input_->IsPressMouse(0)) {
			debugCamera_->Update();
		}

		// Cキーを押すと視角が元の角度に直すようにしてほしい、そして元の角度を上から見下ろすようにしてほしい
		if (input_->TriggerKey(DIK_C)) {
			// 読み取り専用制約を回避するため、一度デバッグカメラのメモリを破棄して再生成
			delete debugCamera_;
			debugCamera_ = new KamataEngine::DebugCamera(WinApp::kWindowWidth, WinApp::kWindowHeight);

			// 再初期化後に一度強制更新をかけてビュー行列を作成する
			debugCamera_->Update();

			// Cキーを押した後のカメラの回転角を真上からの見下ろしに強制リセット
			camera_.rotation_ = {0.0f, 0.0f, 0.0f};
			camera_.translation_ = {0.0f, 0.0f, -30.0f};
			camera_.UpdateMatrix();
		} else {
			// Cキーが押されていない通常時はデバッグカメラの行列をそのまま適用
			camera_.matView = debugCamera_->GetCamera().matView;
			camera_.matProjection = debugCamera_->GetCamera().matProjection;
			camera_.TransferMatrix();
		}
	} else {
		// レールカメラで計算したビュー行列とプロジェクション行列をコピーする
		camera_.matView = cameraController_->GetCamera().matView;
		camera_.matProjection = cameraController_->GetCamera().matProjection;
		camera_.TransferMatrix();
	}

	// 天球の更新
	skydome_->SetPosition({0.0f, player_->GetWorldPosition().y, 0.0f});
	skydome_->Update();

	// ステージの生成(先にやる：プレイヤーが足場を参照する)
	stage_->Update(player_->GetWorldPosition().y);

	player_->Update();

	lighting_->Update(player_->GetWorldPosition());
	background_->Update(player_->GetWorldPosition(), lighting_->GetLightGroup());

	// 撃破・離脱した敵を削除
	enemies_.remove_if([](Enemy* enemy) {
		if (enemy->IsDead()) {
			delete enemy;
			return true;
		}
		return false;
	});

	// 全方位スポーン: 自機を中心とした円周上のランダムな角度に出現
	if (--spawnTimer_ <= 0) {
		spawnTimer_ = kSpawnInterval;
		const float kSpawnRadius = 30.0f; // 画面外になる距離
		float angle = static_cast<float>(rand()) / RAND_MAX * 6.2831853f;
		Vector3 p = player_->GetWorldPosition();
		Vector3 pos = {p.x + std::cos(angle) * kSpawnRadius, p.y + std::sin(angle) * kSpawnRadius, 0.0f};

		Enemy* enemy = new Enemy();
		enemy->Initialize(enemyModel_, enemyTextureHandle_, player_, pos);
		enemies_.push_back(enemy);
	}

	for (Enemy* enemy : enemies_) {
		enemy->Update();
	}

	// ----------------------------------------------------
	// ★ 衝突判定マネージョへの委託処理 (毎フレーム実行)
	// ----------------------------------------------------

	// 1. 前フレームの情報を一度クリアする
	collisionManager_->ClearColliders();

	// 2. 自キャラの登録
	collisionManager_->AddCollider(player_);

	// 3. 自弾リストの登録
	const std::list<PlayerBullet*>& playerBullets = player_->GetBullets();
	for (PlayerBullet* bullet : playerBullets) {
		if (!bullet->IsDead()) {
			collisionManager_->AddCollider(bullet);
		}
	}

	// 4. 敵キャラと敵弾の登録
	for (Enemy* enemy : enemies_) {
		collisionManager_->AddCollider(enemy);
		for (EnemyBullet* bullet : enemy->GetBullets()) {
			if (!bullet->IsDead()) {
				collisionManager_->AddCollider(bullet);
			}
		}
	}

	// 6. マネージャに集約したコライダー全体の当たり判定を一括実行
	collisionManager_->CheckAllCollisions();

	// ----------------------------------------------------
}

void GameScene::Draw() {
	Model::PreDraw();

	// 天球の描画
	skydome_->Draw();

	background_->Draw(camera_);
	stage_->Draw(camera_);

	player_->Draw(&camera_);

	for (Enemy* enemy : enemies_) {
		enemy->Draw(camera_);
	}

	AxisIndicator::GetInstance()->Draw();

	Model::PostDraw();

	// GPUパーティクル（雨）
	rain_->Draw(camera_, player_->GetWorldPosition());
}