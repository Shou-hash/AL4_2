#pragma once
#include "KamataEngine.h"
#include <memory>
#include <string>
#include <vector>

/// <summary>
/// 背景。PictureFrame の板4枚（奥・上・下・左）で箱状の空間を組み、
/// 内側に星(star2)をデータ駆動（CSVテーブル）で各板と同じ向きに最大150個配置し、左側に月(moon)を置く。
/// 手前(-Z)と右(+X)は、カメラがあるので開けてある。箱はプレイヤーに追従する。
/// </summary>
class BackgroundBox {
public:
	void Initialize(KamataEngine::Model* frameModel, KamataEngine::Model* starModel, KamataEngine::Model* moonModel);
	void Update(const KamataEngine::Vector3& playerPos, KamataEngine::LightGroup* lightGroup = nullptr);
	// 内部で Model::PreDraw(kNone)/PostDraw を行う
	void Draw(const KamataEngine::Camera& camera);

private:
	struct Part {
		KamataEngine::WorldTransform worldTransform;
		KamataEngine::Vector3 scale{1, 1, 1};
		KamataEngine::Vector3 rotation{0, 0, 0};
		KamataEngine::Vector3 offset{0, 0, 0};  // 箱の中心からの位置
		KamataEngine::Vector3 forward{0, 0, 1}; // モデルが向いている正面方向ベクトル
		int plateIndex = 0;                     // 属している壁の番号（0:奥(-Z), 1:上(-Y), 2:下(+Y), 3:左(+X)）
	};

	// データ駆動用の星1個分の配置定義（必ず壁の内側に収まる正規化パラメータ）
	struct StarSpawnData {
		int plateIndex = 0; // 0:奥(-Z向き), 1:上(-Y向き), 2:下(+Y向き), 3:左(+X向き)
		float u = 0.0f;     // 壁面内の横位置（-0.90 〜 0.90 にクランプして壁外はみ出しを防止）
		float v = 0.0f;     // 壁面内の縦位置（-0.90 〜 0.90 にクランプして壁外はみ出しを防止）
		float depth = 1.5f; // 壁の内側への浮かし距離（壁貫通を防止）
		float scale = 1.2f; // 星の大きさ倍率
	};

	// 星ポイントライト1個分のオフセット・パラメータ（star_lights.csv から読み込む）
	struct StarLightPart {
		KamataEngine::Vector3 offset{0, 0, 0}; // 箱の中心からの位置（星モデルの座標より少し手前にずらした座標）
		float scale = 1.2f;                    // 照射半径の基準倍率
	};

	void UpdatePart(Part& part, const KamataEngine::Vector3& base);

	// データ駆動テーブルから星の配置データを読み込む（ファイルが無い場合は最大150個分の既定データを生成・保存する）
	std::vector<StarSpawnData> LoadStarLayoutData(const std::string& filePath) const;

	// 星ポイントライト用のCSVテーブルを読み込む（ファイルが無い場合は星の配置データから少し手前にずらした150個分のCSVを新規生成・保存する）
	std::vector<StarSpawnData> LoadStarLightLayoutData(const std::string& filePath, const std::vector<StarSpawnData>& starDataList) const;

	// 星のポイントライト: star_lights.csv から計算した座標を使って最大150個分のポイントライトを更新する
	void UpdateStarLights(KamataEngine::LightGroup* lightGroup);

	KamataEngine::Model* frameModel_ = nullptr;
	KamataEngine::Model* starModel_ = nullptr;
	KamataEngine::Model* moonModel_ = nullptr;

	std::vector<std::unique_ptr<Part>> plates_;
	std::vector<std::unique_ptr<Part>> stars_;
	std::vector<StarLightPart> starLights_;
	Part moon_;

	float centerY_ = 0.0f;
	float starModelRadius_ = 1.0f; // 星モデルのローカル半径
	bool firstUpdate_ = true;
	uint32_t frame_ = 0; // 星の瞬き用のフレーム数

	// ---- 調整用 ----
	static constexpr float kPlateScale = 2.5f;         // 板の拡大率
	static constexpr int kMaxStars = 150;              // 星の最大生成数（150個まで）
	static constexpr int kMaxStarLights = 150;         // 星のポイントライト最大数（星1つにつき1つ）
	static constexpr float kStarMarginUV = 0.90f;      // 壁の端から内側に収める最大UV率（1.0未満で壁の外に出ない）
	static constexpr float kStarDepthMin = 1.0f;       // 壁からの最小距離(内側)
	static constexpr float kStarDepthMax = 3.5f;       // 壁からの最大距離(内側)
	static constexpr float kStarLightDepthMax = 12.0f; // ライトの壁からの最大距離(内側)
	static constexpr float kStarScaleMin = 0.8f;       // 星の大きさ(倍率)
	static constexpr float kStarScaleMax = 2.4f;
	static constexpr float kMoonScale = 12.0f;

	// 星のポイントライト設定
	const KamataEngine::Vector3 starLightColor_ = {1.0f, 0.72f, 0.25f}; // 明るい暖色オレンジ
	static constexpr float kStarLightIntensity = 18.0f;                 // 強度アップ（壁面まで届くように）
	static constexpr float kStarLightRadius = 45.0f;                    // 照射半径を拡大（箱が大きいため）
	static constexpr float kStarLightDecay = 1.0f;                      // 減衰率
	static constexpr float kStarLightDepthOffset = 0.8f;                // 壁のすぐ手前（星の表面付近）に配置して壁面を照らす
	static constexpr int kFirstStarLightIndex = 2;                      // 0=ランタン, 1=月, 2〜=星
	static constexpr float kStarTwinkleAmount = 0.25f;                  // 星のライトの瞬きの幅（0 〜 1）
};