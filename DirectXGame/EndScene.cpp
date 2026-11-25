#include "EndScene.h"
#include "3d/AxisIndicator.h"
#include <string>

using namespace KamataEngine;

EndScene::~EndScene() {
	delete model_;
	delete player_;

	// 天球の解放
	delete skydome_;
	delete modelSkydome_;

	// カメラコントローラの解放
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

void EndScene::Initialize() {
	input_ = Input::GetInstance();
	camera_.Initialize();

	AxisIndicator::GetInstance()->SetVisible(false);

	// プレイヤーの生成と初期化（静止状態にするため Update は呼ばない）
	playerTex_ = TextureManager::Load("uvChecker.png");
	model_ = Model::Create();
	player_ = new Player();
	player_->Initialize(model_, playerTex_);
	player_->UpdateMatrix();

	// 天球の生成と初期化
	modelSkydome_ = Model::CreateFromOBJ("skydome", true);
	skydome_ = new Skydome();
	skydome_->Initialize(modelSkydome_, &camera_);

	// ★ カメラコントローラの生成と初期化（2番目の視点: ThirdPerson に固定）
	cameraController_ = new CameraController();
	cameraController_->Initialize();
	cameraController_->SetTarget(player_);
	cameraController_->SetMode(CameraMode::ThirdPerson);
	cameraController_->Reset();

	// ライティング（夜）
	lighting_ = new LightingManager();
	lighting_->Initialize();
	lighting_->Apply(model_);

	// ステージ（galaxy1〜3）
	for (int i = 0; i < 3; ++i) {
		modelGalaxy_[i] = Model::CreateFromOBJ("galaxy" + std::to_string(i + 1));
		lighting_->Apply(modelGalaxy_[i]);
	}
	stage_ = new StageManager();
	stage_->Initialize({modelGalaxy_[0], modelGalaxy_[1], modelGalaxy_[2]});
	player_->SetStage(stage_);

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

void EndScene::Update() {
	// SPACEキーでタイトルシーンへ戻る
	if (input_->TriggerKey(DIK_SPACE)) {
		isFinished_ = true;
	}

	// ★ プレイヤーを動かさないため UpdateMatrix のみ実行
	player_->UpdateMatrix();

	// ★ カメラの視点を常に2番目（ThirdPerson）に固定して更新
	cameraController_->SetMode(CameraMode::ThirdPerson);
	cameraController_->Update();
	camera_.matView = cameraController_->GetCamera().matView;
	camera_.matProjection = cameraController_->GetCamera().matProjection;
	camera_.TransferMatrix();

	// 天球・ステージ・ライティング・背景の更新（プレイヤー位置基準で配置）
	Vector3 playerPos = player_->GetWorldPosition();
	skydome_->SetPosition({0.0f, playerPos.y, 0.0f});
	skydome_->Update();
	stage_->Update(playerPos.y);
	lighting_->Update(playerPos);
	background_->Update(playerPos, lighting_->GetLightGroup());

	// ★ 敵の生成・更新および player_->Update() は行わない
}

void EndScene::Draw() {
	Model::PreDraw();

	// 天球の描画
	skydome_->Draw();

	background_->Draw(camera_);
	stage_->Draw(camera_);

	// 静止したプレイヤーの描画
	player_->Draw(&camera_);

	Model::PostDraw();

	// GPUパーティクル（雨）
	rain_->Draw(camera_, player_->GetWorldPosition());
}