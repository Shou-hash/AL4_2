#include "BackgroundBox.h"
#include "LightingManager.h"
#include "Matrix4x4.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>

using namespace KamataEngine;

namespace {
const float kPi = 3.14159265f;

float Rand01() { return static_cast<float>(rand()) / RAND_MAX; }
float RandRange(float a, float b) { return a + (b - a) * Rand01(); }

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
				b.maxX = (std::max)(b.maxX, v.pos.y);
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

void BackgroundBox::Initialize(Model* frameModel, Model* starModel, Model* moonModel) {
	frameModel_ = frameModel;
	starModel_ = starModel;
	moonModel_ = moonModel;

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

	for (const PlateDef& d : defs) {
		auto plate = std::make_unique<Part>();
		plate->worldTransform.Initialize();
		plate->scale = d.scale;
		plate->rotation = d.rotation;
		plate->forward = d.normal; // 内向き法線方向をモデルの正面方向とする
		const float back = frontZ * d.scale.z / s;
		plate->offset = {d.face.x - d.normal.x * back, d.face.y - d.normal.y * back, d.face.z - d.normal.z * back};
		plates_.push_back(std::move(plate));

		// 星：この板の内側の面にランダム配置。向きは板と完全に同じ
		for (int i = 0; i < kStarsPerPlate; ++i) {
			auto star = std::make_unique<Part>();
			star->worldTransform.Initialize();
			float k = RandRange(kStarScaleMin, kStarScaleMax);
			star->scale = {k, k, k};
			star->rotation = d.rotation;
			star->forward = d.normal; // 星モデルが向いている正面方向
			float u = RandRange(-0.95f, 0.95f);
			float v = RandRange(-0.95f, 0.95f);
			float depth = RandRange(1.0f, 4.0f); // 板からの距離(内側)
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
			star->offset = {p.x + d.normal.x * depth, p.y + d.normal.y * depth, p.z + d.normal.z * depth};
			stars_.push_back(std::move(star));
		}
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

	// ライトグループが渡されている場合、各星に1つずつオレンジ色のポイントライトを設定
	if (lightGroup) {
		// ランタン(0)・月(1)の次からポイントライトを割り当てる (インデックス 2 以降)
		int lightIndex = 2;
		for (size_t i = 0; i < stars_.size() && lightIndex < LightGroup::kPointLightNum; ++i, ++lightIndex) {
			const auto& star = stars_[i];
			const Vector3 starPos = star->worldTransform.translation_;

			// 星のモデルが向いている方向（forward）へ手前に移動した座標を計算
			float offsetDistance = kStarFrontOffset * star->scale.x;
			Vector3 lightPos = {starPos.x + star->forward.x * offsetDistance, starPos.y + star->forward.y * offsetDistance, starPos.z + star->forward.z * offsetDistance};

			// ポイントライトの設定（オレンジ色・星のスケールに合わせた可変範囲）
			lightGroup->SetPointLightActive(lightIndex, true);
			lightGroup->SetPointLightColor(lightIndex, starLightColor_);
			lightGroup->SetPointLightIntensity(lightIndex, kStarLightIntensity);
			lightGroup->SetPointLightRadius(lightIndex, 10.0f * star->scale.x);
			lightGroup->SetPointLightDecay(lightIndex, 1.5f);
			lightGroup->SetPointLightPos(lightIndex, lightPos);
		}
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