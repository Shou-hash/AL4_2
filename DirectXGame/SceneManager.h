#pragma once
#include "EndScene.h"
#include "GameScene.h"
#include "TitleScene.h"

// シーンの種類
enum class SceneType {
	Title, // タイトルシーン
	Game,  // ゲームシーン
	End,   // エンドシーン
};

/// <summary>
/// シーン管理クラス
/// </summary>
class SceneManager {
public:
	~SceneManager();

	// 初期化（最初のシーンを生成）
	void Initialize();

	// 更新（現在のシーンの更新と切り替え判定）
	void Update();

	// 描画
	void Draw();

private:
	// シーン切り替え処理
	void ChangeScene(SceneType nextScene);

private:
	SceneType currentScene_ = SceneType::Title;

	TitleScene* titleScene_ = nullptr;
	GameScene* gameScene_ = nullptr;
	EndScene* endScene_ = nullptr;
};