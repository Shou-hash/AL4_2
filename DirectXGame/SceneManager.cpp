#include "SceneManager.h"

SceneManager::~SceneManager() {
	delete titleScene_;
	delete gameScene_;
	delete endScene_;
}

void SceneManager::Initialize() {
	// 最初はタイトルシーンから開始
	ChangeScene(SceneType::Title);
}

void SceneManager::ChangeScene(SceneType nextScene) {
	// 既存シーンの解放
	delete titleScene_;
	titleScene_ = nullptr;
	delete gameScene_;
	gameScene_ = nullptr;
	delete endScene_;
	endScene_ = nullptr;

	currentScene_ = nextScene;

	// 次のシーンを生成して初期化
	switch (currentScene_) {
	case SceneType::Title:
		titleScene_ = new TitleScene();
		titleScene_->Initialize();
		break;
	case SceneType::Game:
		gameScene_ = new GameScene();
		gameScene_->Initialize();
		break;
	case SceneType::End:
		endScene_ = new EndScene();
		endScene_->Initialize();
		break;
	}
}

void SceneManager::Update() {
	switch (currentScene_) {
	case SceneType::Title:
		if (titleScene_) {
			titleScene_->Update();
			if (titleScene_->IsFinished()) {
				ChangeScene(SceneType::Game);
			}
		}
		break;

	case SceneType::Game:
		if (gameScene_) {
			gameScene_->Update();
			if (gameScene_->IsFinished()) {
				ChangeScene(SceneType::End);
			}
		}
		break;

	case SceneType::End:
		if (endScene_) {
			endScene_->Update();
			if (endScene_->IsFinished()) {
				ChangeScene(SceneType::Title);
			}
		}
		break;
	}
}

void SceneManager::Draw() {
	switch (currentScene_) {
	case SceneType::Title:
		if (titleScene_) {
			titleScene_->Draw();
		}
		break;

	case SceneType::Game:
		if (gameScene_) {
			gameScene_->Draw();
		}
		break;

	case SceneType::End:
		if (endScene_) {
			endScene_->Draw();
		}
		break;
	}
}