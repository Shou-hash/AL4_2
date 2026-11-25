#include "BackgroundBox.h"
#include "LightingManager.h"
#include "Matrix4x4.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <utility>
#include <vector>

using namespace KamataEngine;

namespace {
const float kPi = 3.14159265f;

// 偏りなく0〜1の値を返す低食い違い列（ハルツォン数列）
float Halton(int index, int base) {
	float f = 1.0f;
	float r = 0.0f;
	int i = index;
	while (i > 0) {
		f /= static_cast<float>(base);
		r += f * static_cast<float>(i % base);
		i /= base;
	}
	return r;
}

struct Bounds {
	float minX = 0, maxX = 0, minY = 0, maxY = 0, minZ = 0, maxZ = 0;
};

Bounds CalcBounds(Model* model) {
	Bounds b;
	bool first = true;
	for (const auto& mesh : model->GetMeshes()) {
		for (const Mesh::VertexPosNormalUv& v : mesh->GetVertices()) {
			if (first) {
				b.minX = b.maxX = v.pos.x;
				b.minY = b.maxY = v.pos.y;
				b.minZ = b.maxZ = v.pos.z;
				first = false;
			} else {
				b.minX = (std::min)(b.minX, v.pos.x);
				b.maxX = (std::max)(b.maxX, v.pos.x);
				b.minY = (std::min)(b.minY, v.pos.y);
				b.maxY = (std::max)(b.maxY, v.pos.y);
				b.minZ = (std::min)(b.minZ, v.pos.z);
				b.maxZ = (std::max)(b.maxZ, v.pos.z);
			}
		}
	}
	assert(!first);
	return b;
}
} // namespace

std::vector<BackgroundBox::StarSpawnData> BackgroundBox::LoadStarLayoutData(const std::string& filePath) const {
	// 50個分の既定配置テーブル（4枚の壁にバランス良く散らしたベースデータ）
	// {plateIndex, u(-0.9〜0.9), v(-0.9〜0.9), depth(1.0〜3.5), scale}
	static const StarSpawnData kBaseTable[50] = {
	    // 0: 奥の板（18個）
	    {0, -0.84f, 0.74f,  1.4f, 1.8f},
	    {0, -0.78f, -0.68f, 2.1f, 1.3f},
	    {0, -0.65f, 0.15f,  1.7f, 1.5f},
	    {0, -0.52f, -0.40f, 2.2f, 1.2f},
	    {0, -0.44f, 0.62f,  2.6f, 1.9f},
	    {0, -0.35f, 0.25f,  1.8f, 2.1f},
	    {0, -0.24f, -0.76f, 1.5f, 1.4f},
	    {0, -0.10f, 0.72f,  2.8f, 1.1f},
	    {0, -0.04f, -0.18f, 2.0f, 1.7f},
	    {0, 0.05f,  -0.60f, 1.5f, 1.6f},
	    {0, 0.16f,  0.48f,  2.4f, 1.2f},
	    {0, 0.28f,  0.12f,  2.5f, 2.2f},
	    {0, 0.38f,  -0.44f, 1.9f, 1.5f},
	    {0, 0.48f,  0.58f,  1.3f, 1.3f},
	    {0, 0.58f,  0.05f,  2.7f, 1.8f},
	    {0, 0.66f,  -0.22f, 2.0f, 1.9f},
	    {0, 0.75f,  0.70f,  1.6f, 1.1f},
	    {0, 0.82f,  -0.75f, 1.7f, 1.0f},
	    // 1: 上の板（12個）
	    {1, -0.82f, 0.42f,  1.5f, 1.4f},
	    {1, -0.72f, -0.45f, 1.6f, 1.5f},
	    {1, -0.56f, 0.10f,  2.2f, 1.8f},
	    {1, -0.40f, 0.35f,  2.4f, 2.0f},
	    {1, -0.22f, -0.66f, 1.8f, 1.3f},
	    {1, -0.08f, -0.20f, 1.3f, 1.2f},
	    {1, 0.08f,  0.68f,  2.5f, 1.6f},
	    {1, 0.22f,  0.55f,  2.7f, 1.7f},
	    {1, 0.39f,  -0.12f, 1.4f, 1.9f},
	    {1, 0.55f,  -0.62f, 1.9f, 1.4f},
	    {1, 0.68f,  0.44f,  2.3f, 1.3f},
	    {1, 0.76f,  0.18f,  2.1f, 1.1f},
	    // 2: 下の板（10個）
	    {2, -0.80f, -0.38f, 1.6f, 1.5f},
	    {2, -0.68f, 0.42f,  1.8f, 1.3f},
	    {2, -0.48f, -0.12f, 2.1f, 1.7f},
	    {2, -0.25f, -0.50f, 2.3f, 1.9f},
	    {2, -0.05f, 0.64f,  1.5f, 1.2f},
	    {2, 0.12f,  0.28f,  1.4f, 1.5f},
	    {2, 0.30f,  -0.68f, 2.4f, 1.8f},
	    {2, 0.46f,  -0.15f, 2.6f, 1.2f},
	    {2, 0.62f,  0.22f,  1.9f, 1.4f},
	    {2, 0.74f,  0.60f,  1.7f, 2.0f},
	    // 3: 左の板（10個）
	    {3, -0.78f, -0.42f, 1.7f, 1.4f},
	    {3, -0.62f, 0.52f,  1.5f, 1.7f},
	    {3, -0.45f, 0.08f,  2.0f, 1.3f},
	    {3, -0.28f, -0.35f, 2.2f, 1.2f},
	    {3, -0.10f, 0.66f,  1.6f, 1.8f},
	    {3, 0.08f,  0.15f,  1.9f, 2.1f},
	    {3, 0.25f,  -0.22f, 2.4f, 1.5f},
	    {3, 0.42f,  -0.58f, 2.5f, 1.4f},
	    {3, 0.58f,  0.10f,  1.8f, 1.9f},
	    {3, 0.70f,  0.40f,  1.3f, 1.6f},
	};

	std::vector<StarSpawnData> result;
	result.reserve(kMaxStars);

	std::ifstream ifs(filePath);
	if (ifs.is_open()) {
		std::string line;
		while (std::getline(ifs, line) && static_cast<int>(result.size()) < kMaxStars) {
			if (line.empty() || line[0] == '#') {
				continue;
			}
			std::stringstream ss(line);
			std::string token;
			StarSpawnData data{};
			if (std::getline(ss, token, ','))
				data.plateIndex = std::stoi(token);
			if (std::getline(ss, token, ','))
				data.u = std::stof(token);
			if (std::getline(ss, token, ','))
				data.v = std::stof(token);
			if (std::getline(ss, token, ','))
				data.depth = std::stof(token);
			if (std::getline(ss, token, ','))
				data.scale = std::stof(token);
			result.push_back(data);
		}
	}

	// ファイルが無い、または行数が150個に満たない場合は既定データで補完し、CSVを自動生成する
	if (static_cast<int>(result.size()) < kMaxStars) {
		// 4枚の板の割り当て比率（奥:18, 上:12, 下:10, 左:10 の50個パターンを3周分）
		static const int kPlatePattern[5] = {0, 0, 1, 2, 3};
		for (int i = static_cast<int>(result.size()); i < kMaxStars; ++i) {
			if (i < 50) {
				result.push_back(kBaseTable[i]);
			} else {
				const int seq = i - 49; // 1 〜 100
				StarSpawnData d{};
				d.plateIndex = kPlatePattern[i % 5];
				d.u = (Halton(seq, 2) * 2.0f - 1.0f) * kStarMarginUV;
				d.v = (Halton(seq, 3) * 2.0f - 1.0f) * kStarMarginUV;
				d.depth = kStarDepthMin + (kStarDepthMax - kStarDepthMin) * Halton(seq, 5);
				d.scale = kStarScaleMin + (kStarScaleMax - kStarScaleMin) * Halton(seq, 7);
				result.push_back(d);
			}
		}
		std::filesystem::path p(filePath);
		if (p.has_parent_path()) {
			std::filesystem::create_directories(p.parent_path());
		}
		std::ofstream ofs(filePath);
		if (ofs.is_open()) {
			ofs << "# plateIndex(0-3), u(-0.90..0.90), v(-0.90..0.90), depth(1.0..3.5), scale (max 150 rows)\n";
			for (const auto& d : result) {
				ofs << d.plateIndex << ", " << d.u << ", " << d.v << ", " << d.depth << ", " << d.scale << "\n";
			}
		}
	}

	return result;
}

std::vector<BackgroundBox::StarSpawnData> BackgroundBox::LoadStarLightLayoutData(const std::string& filePath, const std::vector<StarSpawnData>& starDataList) const {
	std::vector<StarSpawnData> result;
	result.reserve(kMaxStarLights);

	bool needRegenerate = false;
	int plateCounts[4] = {0, 0, 0, 0};

	std::ifstream ifs(filePath);
	if (ifs.is_open()) {
		std::string line;
		while (std::getline(ifs, line) && static_cast<int>(result.size()) < kMaxStarLights) {
			if (line.empty() || line[0] == '#') {
				continue;
			}
			std::stringstream ss(line);
			std::string token;
			StarSpawnData data{};
			if (std::getline(ss, token, ','))
				data.plateIndex = std::stoi(token);
			if (std::getline(ss, token, ','))
				data.u = std::stof(token);
			if (std::getline(ss, token, ','))
				data.v = std::stof(token);
			if (std::getline(ss, token, ','))
				data.depth = std::stof(token);
			if (std::getline(ss, token, ','))
				data.scale = std::stof(token);

			if (data.plateIndex >= 0 && data.plateIndex < 4) {
				plateCounts[data.plateIndex]++;
			}
			result.push_back(data);
		}
	}

	const int targetCount = (std::min)(static_cast<int>(starDataList.size()), kMaxStarLights);

	// 150個に満たない場合、または4枚の板のどれか1つでもライトが0個（古い偏ったCSV）の場合は再生成する
	if (static_cast<int>(result.size()) < targetCount || plateCounts[0] == 0 || plateCounts[1] == 0 || plateCounts[2] == 0 || plateCounts[3] == 0) {
		needRegenerate = true;
	}

	if (needRegenerate) {
		result.clear();
		for (int i = 0; i < targetCount; ++i) {
			const StarSpawnData& baseStar = starDataList[static_cast<size_t>(i)];
			StarSpawnData lightData = baseStar;
			// 星モデルの表面のすぐ外側（壁からも近すぎず遠すぎない位置）に配置して壁面と星の両方を照らす
			lightData.depth = baseStar.depth + starModelRadius_ * baseStar.scale * 0.5f + kStarLightDepthOffset;
			result.push_back(lightData);
		}
		std::filesystem::path p(filePath);
		if (p.has_parent_path()) {
			std::filesystem::create_directories(p.parent_path());
		}
		std::ofstream ofs(filePath);
		if (ofs.is_open()) {
			ofs << "# plateIndex(0-3), u(-0.90..0.90), v(-0.90..0.90), depth, scale (auto-generated 150 lights)\n";
			for (const auto& d : result) {
				ofs << d.plateIndex << ", " << d.u << ", " << d.v << ", " << d.depth << ", " << d.scale << "\n";
			}
		}
	}

	return result;
}

void BackgroundBox::Initialize(Model* frameModel, Model* starModel, Model* moonModel) {
	frameModel_ = frameModel;
	starModel_ = starModel;
	moonModel_ = moonModel;

	// 星モデルのローカル半径を計算（ポイントライトが星モデルのメッシュ内部に埋もれないよう表面外へ押し出す基準）
	const Bounds sb = CalcBounds(starModel);
	const float starHalfX = (std::max)(std::abs(sb.minX), std::abs(sb.maxX));
	const float starHalfY = (std::max)(std::abs(sb.minY), std::abs(sb.maxY));
	const float starHalfZ = (std::max)(std::abs(sb.minZ), std::abs(sb.maxZ));
	starModelRadius_ = (std::max)({starHalfX, starHalfY, starHalfZ});

	// 板のサイズ(モデルの頂点から計算)。A=長辺(X)、B=短辺(Y)。板の正面はローカル+Z
	const Bounds fb = CalcBounds(frameModel);
	const float s = kPlateScale;
	const float A = (fb.maxX - fb.minX) * s;
	const float B = (fb.maxY - fb.minY) * s;
	const float frontZ = fb.maxZ * s; // 正面までの距離(スケール後)

	// 箱の半径。X方向=A、Y方向=B、Z方向=B（上下の板の短辺がZ方向を作る）
	const float hx = A * 0.5f;
	const float hy = B * 0.5f;
	const float hz = B * 0.5f;

	// 板：{内向き法線, 回転, スケール, 箱の中心から板の面までのオフセット}
	struct PlateDef {
		Vector3 normal;
		Vector3 rotation;
		Vector3 scale;
		Vector3 face; // 箱の中心から、板の面(内側)への位置
	};
	const PlateDef defs[4] = {
	    {{0, 0, -1}, {0, kPi, 0},         {s, s, s},         {0, 0, hz} },
	    {{0, -1, 0}, {kPi * 0.5f, 0, 0},  {s, s, s},         {0, hy, 0} },
	    {{0, 1, 0},  {-kPi * 0.5f, 0, 0}, {s, s, s},         {0, -hy, 0}},
	    {{1, 0, 0},  {0, kPi * 0.5f, 0},  {s * B / A, s, s}, {-hx, 0, 0}},
	};

	for (int idx = 0; idx < 4; ++idx) {
		const PlateDef& d = defs[idx];
		auto plate = std::make_unique<Part>();
		plate->worldTransform.Initialize();
		plate->scale = d.scale;
		plate->rotation = d.rotation;
		plate->forward = d.normal; // 内向き法線方向をモデルの正面方向とする
		plate->plateIndex = idx;
		const float back = frontZ * d.scale.z / s;
		plate->offset = {d.face.x - d.normal.x * back, d.face.y - d.normal.y * back, d.face.z - d.normal.z * back};
		plates_.push_back(std::move(plate));
	}

	// 1. データ駆動テーブル（stars.csv）から最大150個分の星配置を読み込み、必ず壁の内側に収まるようクランプして生成する
	const std::vector<StarSpawnData> starDataList = LoadStarLayoutData("Resources/data/stars.csv");
	const int starCount = (std::min)(static_cast<int>(starDataList.size()), kMaxStars);
	for (int i = 0; i < starCount; ++i) {
		const StarSpawnData& sd = starDataList[static_cast<size_t>(i)];

		// 壁の外へ絶対に出ないようインデックス・面内座標・内側オフセットを安全範囲に制限する
		const int plateIdx = std::clamp(sd.plateIndex, 0, 3);
		const PlateDef& d = defs[plateIdx];
		const float u = std::clamp(sd.u, -kStarMarginUV, kStarMarginUV);
		const float v = std::clamp(sd.v, -kStarMarginUV, kStarMarginUV);
		const float maxSafeDepth = (std::min)(kStarDepthMax, (std::min)(hx, (std::min)(hy, hz)) * 0.5f);
		const float depth = std::clamp(sd.depth, kStarDepthMin, maxSafeDepth);
		const float k = std::clamp(sd.scale, kStarScaleMin, kStarScaleMax);

		auto star = std::make_unique<Part>();
		star->worldTransform.Initialize();
		star->scale = {k, k, k};
		star->rotation = d.rotation;
		star->forward = d.normal; // 星モデルが向いている正面方向
		star->plateIndex = plateIdx;

		Vector3 p = d.face;
		if (d.normal.z != 0.0f) { // 奥の板：XY平面
			p.x = u * hx;
			p.y = v * hy;
		} else if (d.normal.y != 0.0f) { // 上下の板：XZ平面
			p.x = u * hx;
			p.z = v * hz;
		} else { // 左の板：YZ平面
			p.y = v * hy;
			p.z = u * hz;
		}
		// 法線（箱の内向き）方向に depth だけ進めるため、必ず壁の内側空間に配置される
		star->offset = {p.x + d.normal.x * depth, p.y + d.normal.y * depth, p.z + d.normal.z * depth};
		stars_.push_back(std::move(star));
	}

	// 2. 星ポイントライト用の新しいCSV（star_lights.csv）を読み込み（無ければ星より手前にずらした150個分を新規作成）、ライトのオフセット座標を構築する
	const std::vector<StarSpawnData> starLightDataList = LoadStarLightLayoutData("Resources/data/star_lights.csv", starDataList);
	const int starLightCount = (std::min)(static_cast<int>(starLightDataList.size()), kMaxStarLights);
	starLights_.reserve(static_cast<size_t>(starLightCount));
	for (int i = 0; i < starLightCount; ++i) {
		const StarSpawnData& ld = starLightDataList[static_cast<size_t>(i)];

		const int plateIdx = std::clamp(ld.plateIndex, 0, 3);
		const PlateDef& d = defs[plateIdx];
		const float u = std::clamp(ld.u, -kStarMarginUV, kStarMarginUV);
		const float v = std::clamp(ld.v, -kStarMarginUV, kStarMarginUV);
		const float maxSafeLightDepth = (std::min)(kStarLightDepthMax, (std::min)(hx, (std::min)(hy, hz)) * 0.8f);
		const float depth = std::clamp(ld.depth, kStarDepthMin, maxSafeLightDepth);
		const float k = std::clamp(ld.scale, kStarScaleMin, kStarScaleMax);

		Vector3 p = d.face;
		if (d.normal.z != 0.0f) { // 奥の板：XY平面
			p.x = u * hx;
			p.y = v * hy;
		} else if (d.normal.y != 0.0f) { // 上下の板：XZ平面
			p.x = u * hx;
			p.z = v * hz;
		} else { // 左の板：YZ平面
			p.y = v * hy;
			p.z = u * hz;
		}

		StarLightPart lp{};
		lp.offset = {p.x + d.normal.x * depth, p.y + d.normal.y * depth, p.z + d.normal.z * depth};
		lp.scale = k;
		starLights_.push_back(lp);
	}

	// 月：X軸に 90度 (kPi * 0.5f) 回転を加えて縦に立たせる
	moon_.worldTransform.Initialize();
	moon_.scale = {kMoonScale, kMoonScale, kMoonScale};
	moon_.rotation = {kPi * 0.5f, kPi, 0.0f};
	moon_.forward = {0.0f, 0.0f, -1.0f};
}

void BackgroundBox::UpdatePart(Part& part, const Vector3& base) {
	WorldTransform& wt = part.worldTransform;
	wt.scale_ = part.scale;
	wt.rotation_ = part.rotation;
	wt.translation_ = {base.x + part.offset.x, base.y + part.offset.y, base.z + part.offset.z};
	wt.matWorld_ = MakeAffineMatrix(wt.scale_, wt.rotation_, wt.translation_);
	wt.TransferMatrix();
}

void BackgroundBox::Update(const Vector3& playerPos, LightGroup* lightGroup) {
	// 箱の中心：Xはプレイヤーに追従、Yはゆるやかに追従
	if (firstUpdate_) {
		centerY_ = playerPos.y;
		firstUpdate_ = false;
	}
	centerY_ += (playerPos.y - centerY_) * 0.05f;
	const Vector3 center = {playerPos.x, centerY_, 0.0f};

	for (auto& plate : plates_) {
		UpdatePart(*plate, center);
	}
	for (auto& star : stars_) {
		UpdatePart(*star, center);
	}

	// 月：ライトの位置と同じ場所（左・上・空）
	moon_.offset = {0, 0, 0};
	UpdatePart(moon_, LightingManager::GetMoonPosition(playerPos));

	// ライトグループが渡されている場合、star_lights.csv から計算した座標で星のポイントライトを更新する
	++frame_;
	if (lightGroup) {
		UpdateStarLights(lightGroup);
		lightGroup->Update(); // ★ 星のライトを変えたあと、すぐ転送する(LightingManager::Update のあとに呼ばれるので、これが無いと 1 フレーム遅れる)
	}
}

void BackgroundBox::UpdateStarLights(LightGroup* lightGroup) {

	const int availableLights = static_cast<int>(LightGroup::kPointLightNum) - kFirstStarLightIndex;
	const int activeStarLightCount = (std::min)({availableLights, kMaxStarLights, static_cast<int>(starLights_.size())});

	if (availableLights <= 0) {
		return;
	}

	// star_lights.csv から生成されたポイントライト（最大150個）を有効化する
	
	const Vector3 center = {plates_.empty() ? 0.0f : (plates_[0]->worldTransform.translation_.x - plates_[0]->offset.x), centerY_, 0.0f};

	for (int i = 0; i < activeStarLightCount; ++i) {
		const int lightIndex = kFirstStarLightIndex + i;
		const StarLightPart& lp = starLights_[static_cast<size_t>(i)];

		// star_lights.csv から計算されたオフセット座標に箱の中心を加えてワールド座標にする
		const Vector3 lightPos = {center.x + lp.offset.x, center.y + lp.offset.y, center.z + lp.offset.z};

		// 星ごとに、位相をずらして、ゆっくり瞬く
		const float twinkle = 1.0f + std::sin(static_cast<float>(frame_) * 0.05f + static_cast<float>(i) * 1.7f) * kStarTwinkleAmount;

		lightGroup->SetPointLightActive(lightIndex, true);
		lightGroup->SetPointLightColor(lightIndex, starLightColor_);
		lightGroup->SetPointLightIntensity(lightIndex, kStarLightIntensity * twinkle);
		lightGroup->SetPointLightRadius(lightIndex, kStarLightRadius * lp.scale);
		lightGroup->SetPointLightDecay(lightIndex, kStarLightDecay);
		lightGroup->SetPointLightPos(lightIndex, lightPos);
	}

	// 余ったポイントライトスロットがある場合は無効化する
	for (int i = activeStarLightCount; i < availableLights; ++i) {
		lightGroup->SetPointLightActive(kFirstStarLightIndex + i, false);
	}
}

void BackgroundBox::Draw(const Camera& camera) {
	// 薄い板・星・月なので両面描画（向きに依存しない）
	Model::PreDraw(Model::CullingMode::kNone);
	for (auto& plate : plates_) {
		frameModel_->Draw(plate->worldTransform, camera);
	}
	for (auto& star : stars_) {
		starModel_->Draw(star->worldTransform, camera);
	}
	moonModel_->Draw(moon_.worldTransform, camera);
	Model::PostDraw();
}