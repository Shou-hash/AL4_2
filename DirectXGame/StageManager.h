#pragma once
#include "KamataEngine.h"
#include <list>
#include <memory>
#include <vector>

class EffectParticleSystem;

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

	// ステージ裏面パーティクル用のエフェクトシステムを設定
	void SetEffectParticleSystem(EffectParticleSystem* effectParticles) { effectParticles_ = effectParticles; }

	// プレイヤーのY座標に合わせて前方に生成し、後方は再利用する（揺れ更新も行う）
	void Update(float playerY, float playerZ = -kPlayerHalfHeight);

	// ★ 追加: プレイヤーがジャンプ後にステージへ着地した瞬間に上下揺れ＆裏面パーティクルを発火させる
	void TriggerLandingShake(float playerY);

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
		float minY = 0, maxY = 0;     // ワールドでの足場のY範囲
		float baseZ = 0.0f;           // 揺れ基準のZ座標
		float shakeAmp = 0.0f;        // 上下揺れの振幅
		float shakeTime = 0.0f;       // 揺れ経過時間
		bool arrived = false;         // プレイヤーが、このステージに、到着(初めて着地)したか
		float backEffectTime = 0.0f;  // 裏面のエフェクトを、あと何秒出すか(到着してから、しばらくの間だけ出す)
		float backEffectTimer = 0.0f; // 次のエフェクトまでの秒数
	};

	void BuildProfile(ModelInfo& info);
	void Place(Stage& stage);
	// ステージの裏面(+Z 側の面)から、パーティクルを出す。centerY の前後の範囲に、横幅いっぱいに散らばして出す
	void EmitBackEffect(const Stage& stage, float centerY, int burstCount, int particlesPerBurst);

	// ---- 調整用 ----
	static constexpr float kGroundZ = 0.0f;        // 各ステージの「手前端の上面」を合わせる高さ(Z座標)
	static constexpr float kStageScale = 1.0f;     // ステージの拡大率
	static constexpr float kProfileStep = 0.25f;   // 高さプロファイルの刻み
	static constexpr float kGapMin = 4.0f;         // 隙間の幅(最小)
	static constexpr float kGapMax = 7.0f;         // 隙間の幅(最大)。ジャンプで越えられる距離(約11)未満にする
	static constexpr float kSpawnAhead = 150.0f;   // どこまで先に生成するか
	static constexpr float kRemoveBehind = 100.0f; // どこまで後ろを残すか

	// ---- ステージ裏面のエフェクト（到着したときだけ出る）----
	static constexpr float kBackEffectDuration = 1.2f;    // 到着してから、エフェクトを出し続ける秒数
	static constexpr float kBackEffectInterval = 0.08f;   // 出し続けている間の、間隔(秒)
	static constexpr int kBackEffectArrivalBursts = 4;    // 到着した瞬間に出す、まとまりの数(横幅に散らばる)
	static constexpr int kBackEffectBursts = 2;           // 出し続けている間の、1 回あたりのまとまりの数
	static constexpr int kBackEffectParticles = 48;       // 1 まとまりあたりの粒の数
	static constexpr float kBackEffectRangeBehind = 4.0f; // プレイヤーの Y から、後ろへの範囲(メートル)
	static constexpr float kBackEffectRangeAhead = 12.0f; // プレイヤーの Y から、前への範囲(メートル)
	static constexpr float kBackEffectDirectionZ = -1.0f; // 粒の飛ぶ向き(-1 = 空側へ、ステージの面を抜けて上がる・+1 = 地面側へ)

	std::vector<ModelInfo> infos_;
	std::list<std::unique_ptr<Stage>> stages_; // Y昇順
	float cursor_ = -10.0f;                    // 次の足場の開始Y
	size_t counter_ = 0;
	EffectParticleSystem* effectParticles_ = nullptr;
};