#pragma once
#include "KamataEngine.h"
#include <list>
#include <memory>
#include <vector>

/// <summary>
/// ステージ管理。galaxy1〜3 を隙間を空けて上(+Y)へ無限に並べる。
/// 足場の高さ(Z軸)はモデルの頂点(上面の輪郭)から実行時に求める。
/// </summary>
class StageManager {
public:
	// プレイヤー(立方体)の中心から足の裏までの高さ
	static constexpr float kPlayerHalfHeight = 1.0f;

	// models: ステージ用モデル（この順番で繰り返し並ぶ）
	void Initialize(const std::vector<KamataEngine::Model*>& models);
	// プレイヤーのY座標に合わせて前方に生成し、後方は再利用する
	void Update(float playerY);
	// 内部で Model::PreDraw(kNone)/PostDraw を行う
	void Draw(const KamataEngine::Camera& camera);

	// y の真下にある足場上面の高さ(Z座標)。足場が無い(隙間)なら false
	bool TryGetGroundZ(float y, float& outZ) const;
	// 穴に落ちたときの復帰位置（落ちた隙間の手前の足場の先頭）
	KamataEngine::Vector3 GetRespawnPosition(float y) const;

private:
	struct ModelInfo {
		KamataEngine::Model* model = nullptr;
		// モデル(ローカル)の境界ボックス
		float minX = 0, maxX = 0, minY = 0, maxY = 0, minZ = 0, maxZ = 0;
		// 上面の高さプロファイル（minXから kProfileStep ごと。足場が無い所は NaN）
		std::vector<float> top;
		// 端の上面の高さ(Z)
		float frontTop = 0;
	};
	struct Stage {
		size_t modelIndex = 0;
		KamataEngine::WorldTransform worldTransform;
		float minY = 0, maxY = 0; // ワールドでの足場のY範囲
	};

	void BuildProfile(ModelInfo& info);
	void Place(Stage& stage);

	// ---- 調整用 ----
	static constexpr float kGroundZ = 0.0f;        // 各ステージの「手前端の上面」を合わせる高さ(Z座標)
	static constexpr float kStageScale = 1.0f;     // ステージの拡大率
	static constexpr float kProfileStep = 0.25f;   // 高さプロファイルの刻み
	static constexpr float kGapMin = 4.0f;         // 隙間の幅(最小)
	static constexpr float kGapMax = 7.0f;         // 隙間の幅(最大)。ジャンプで越えられる距離(約11)未満にする
	static constexpr float kSpawnAhead = 150.0f;   // どこまで先に生成するか
	static constexpr float kRemoveBehind = 100.0f; // どこまで後ろを残すか

	std::vector<ModelInfo> infos_;
	std::list<std::unique_ptr<Stage>> stages_; // Y昇順
	float cursor_ = -10.0f;                    // 次の足場の開始Y
	size_t counter_ = 0;
};