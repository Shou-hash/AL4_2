#include "StageManager.h"
#include "EffectParticleSystem.h"
#include "Matrix4x4.h"
#include <algorithm>
#include <cassert>
#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <limits>

using namespace KamataEngine;

void StageManager::Initialize(const std::vector<Model*>& models) {
	assert(!models.empty());

	for (Model* model : models) {
		ModelInfo info;
		info.model = model;
		bool first = true;
		for (const auto& mesh : model->GetMeshes()) {
			for (const Mesh::VertexPosNormalUv& v : mesh->GetVertices()) {
				if (first) {
					info.minX = info.maxX = v.pos.x;
					info.minY = info.maxY = v.pos.y;
					info.minZ = info.maxZ = v.pos.z;
					first = false;
				} else {
					info.minX = (std::min)(info.minX, v.pos.x);
					info.maxX = (std::max)(info.maxX, v.pos.x);
					info.minY = (std::min)(info.minY, v.pos.y);
					info.maxY = (std::max)(info.maxY, v.pos.y);
					info.minZ = (std::min)(info.minZ, v.pos.z);
					info.maxZ = (std::max)(info.maxZ, v.pos.z);
				}
			}
		}
		assert(!first && "ステージモデルに頂点がありません");
		BuildProfile(info);
		infos_.push_back(std::move(info));
	}

	cursor_ = -10.0f;
	counter_ = 0;
}

// 三角形を正面から見た輪郭から、進行方向(ローカルX軸)ごとの最も高いZ点を求める
void StageManager::BuildProfile(ModelInfo& info) {
	const int n = static_cast<int>((info.maxX - info.minX) / kProfileStep) + 2;
	info.top.assign(n, -FLT_MAX);

	for (const auto& mesh : info.model->GetMeshes()) {
		const auto& vtx = mesh->GetVertices();
		const auto& idx = mesh->GetIndices();
		for (size_t t = 0; t + 2 < idx.size(); t += 3) {
			const Vector3 p[3] = {vtx[idx[t]].pos, vtx[idx[t + 1]].pos, vtx[idx[t + 2]].pos};
			float xmin = (std::min)({p[0].x, p[1].x, p[2].x});
			float xmax = (std::max)({p[0].x, p[1].x, p[2].x});
			int i0 = (std::max)(0, static_cast<int>(std::ceil((xmin - info.minX) / kProfileStep)));
			int i1 = (std::min)(n - 1, static_cast<int>(std::floor((xmax - info.minX) / kProfileStep)));
			for (int i = i0; i <= i1; ++i) {
				float x = info.minX + i * kProfileStep;
				for (int e = 0; e < 3; ++e) {
					const Vector3& a = p[e];
					const Vector3& b = p[(e + 1) % 3];
					if ((a.x - x) * (b.x - x) > 0.0f || a.x == b.x) {
						continue;
					}
					float s = (x - a.x) / (b.x - a.x);
					info.top[i] = (std::max)(info.top[i], a.z + (b.z - a.z) * s);
				}
			}
		}
	}
	for (float& h : info.top) {
		if (h == -FLT_MAX) {
			h = std::numeric_limits<float>::quiet_NaN();
		}
	}
	info.frontTop = std::isnan(info.top[0]) ? info.maxZ : info.top[0];
}

void StageManager::TriggerLandingShake(float playerY) {
	// ステージの着地上下揺れ定数
	constexpr float kLandingShakeAmp = 0.75f;

	for (const auto& stage : stages_) {
		if (playerY < stage->minY || playerY > stage->maxY) {
			continue;
		}
		stage->shakeAmp = kLandingShakeAmp;
		stage->shakeTime = 0.0f;

		// ★ 変更: 裏面のエフェクトは、そのステージに初めて着地したとき(到着したとき)だけ出す。ジャンプのたびには、出さない
		if (!stage->arrived) {
			stage->arrived = true;
			stage->backEffectTime = kBackEffectDuration;
			stage->backEffectTimer = 0.0f;
			EmitBackEffect(*stage, playerY, kBackEffectArrivalBursts, kBackEffectParticles); // 到着した瞬間: 横幅いっぱいに、まとめて出す
		}
		break;
	}
}

void StageManager::EmitBackEffect(const Stage& stage, float centerY, int burstCount, int particlesPerBurst) {
	if (!effectParticles_) {
		return;
	}
	const ModelInfo& m = infos_[stage.modelIndex];

	// 裏面の Z: ワールド Z = 原点の Z − モデルの Z (Y 軸 180 度回転で、Z が反転する)。モデルの一番奥(minZ)の面の、少し裏
	const float backZ = stage.baseZ - m.minZ * kStageScale + 0.4f;
	// 横幅: ステージの中心は X = 0。幅の 9 割の範囲に散らばす
	const float halfWidth = (m.maxY - m.minY) * 0.5f * kStageScale * 0.9f;

	for (int i = 0; i < burstCount; ++i) {
		const float rx = static_cast<float>(rand()) / RAND_MAX * 2.0f - 1.0f; // -1 〜 1
		const float ry = static_cast<float>(rand()) / RAND_MAX;               // 0 〜 1
		const float x = rx * halfWidth;
		float y = centerY - kBackEffectRangeBehind + ry * (kBackEffectRangeBehind + kBackEffectRangeAhead);
		y = (std::min)((std::max)(y, stage.minY), stage.maxY); // このステージの範囲に収める
		effectParticles_->EmitStageBack({x, y, backZ}, particlesPerBurst, kBackEffectDirectionZ);
	}
}

void StageManager::Update(float playerY, float playerZ) {
	(void)playerZ;

	while (cursor_ < playerY + kSpawnAhead) {
		if (stages_.size() > 1 && stages_.front()->maxY < playerY - kRemoveBehind) {
			// 破棄せず先頭を末尾へ回して再利用（GPUが使用中のリソースを解放しないため）
			stages_.splice(stages_.end(), stages_, stages_.begin());
			Place(*stages_.back());
		} else {
			stages_.push_back(std::make_unique<Stage>());
			stages_.back()->worldTransform.Initialize();
			Place(*stages_.back());
		}
	}

	// ★ 追加: 到着したステージの裏面から、しばらくの間、パーティクルを出し続ける
	constexpr float kEffectDt = 1.0f / 60.0f;
	for (const auto& stage : stages_) {
		if (stage->backEffectTime <= 0.0f) {
			continue;
		}
		stage->backEffectTime -= kEffectDt;
		stage->backEffectTimer -= kEffectDt;
		if (stage->backEffectTimer <= 0.0f) {
			stage->backEffectTimer = kBackEffectInterval;
			EmitBackEffect(*stage, playerY, kBackEffectBursts, kBackEffectParticles);
		}
	}

	// ステージのブロックを描画する、カメラからの縦方向の距離（これを超えたら更新しない）
	constexpr float kStageDrawRangeY = 80.0f;
	constexpr float kShakeFrequency = 34.0f;
	const float dt = 1.0f / 60.0f;

	// ステージタイルのワールド行列更新（着地時上下揺れ）
	for (const auto& stage : stages_) {
		if (!stage) {
			continue;
		}
		if (std::fabs(stage->worldTransform.translation_.y - playerY) > kStageDrawRangeY) {
			continue;
		}

		if (stage->shakeAmp > 0.001f) {
			stage->shakeTime += dt;
			stage->shakeAmp *= std::pow(0.06f, dt); // 約0.6秒で滑らかに停止
			float offsetZ = std::sin(stage->shakeTime * kShakeFrequency) * stage->shakeAmp;

			WorldTransform& wt = stage->worldTransform;
			wt.translation_.z = stage->baseZ + offsetZ;
			Matrix4x4 affineMatrix = MakeAffineMatrix(wt.scale_, wt.rotation_, wt.translation_);
			wt.matWorld_ = affineMatrix;
			wt.TransferMatrix();
		} else if (stage->shakeAmp > 0.0f) {
			stage->shakeAmp = 0.0f;
			WorldTransform& wt = stage->worldTransform;
			wt.translation_.z = stage->baseZ;
			Matrix4x4 affineMatrix = MakeAffineMatrix(wt.scale_, wt.rotation_, wt.translation_);
			wt.matWorld_ = affineMatrix;
			wt.TransferMatrix();
		}
	}
}

void StageManager::Place(Stage& stage) {
	stage.modelIndex = counter_++ % infos_.size();
	const ModelInfo& m = infos_[stage.modelIndex];
	const float s = kStageScale;

	stage.minY = cursor_;
	// Z軸90度回転によりローカルX軸の幅(maxX - minX)がワールドY軸方向の長さになる
	stage.maxY = cursor_ + (m.maxX - m.minX) * s;

	WorldTransform& wt = stage.worldTransform;
	wt.scale_ = {s, s, s};
	// ★ モデルの表裏を反転(Y軸180度)＋Z軸90度(1.5707963f)回転を設定
	wt.rotation_ = {0.0f, 3.14159265f, 1.5707963f};

	// 手前端をcursor_に、上面をkGroundZに、横幅(X)の中心をX=0に合わせる
	wt.translation_ = {(m.minY + m.maxY) * 0.5f * s, stage.minY + m.maxX * s, kGroundZ + m.frontTop * s};

	// ★ 追加: 上下揺れの基準Z座標を保存し、揺れ状態をリセット
	stage.baseZ = wt.translation_.z;
	stage.shakeAmp = 0.0f;
	stage.shakeTime = 0.0f;
	stage.arrived = false; // 再利用されたステージは、また「到着」できる
	stage.backEffectTime = 0.0f;
	stage.backEffectTimer = 0.0f;

	// ★ MakeAffineMatrix で回転を含めて行列を生成
	wt.matWorld_ = MakeAffineMatrix(wt.scale_, wt.rotation_, wt.translation_);
	wt.TransferMatrix();

	// 次の足場までの隙間（ランダム）
	float gap = kGapMin + (kGapMax - kGapMin) * (static_cast<float>(rand()) / RAND_MAX);
	cursor_ = stage.maxY + gap;
}

void StageManager::Draw(const Camera& camera) {
	// 薄い板なので両面描画（向きに依存しない）
	Model::PreDraw(Model::CullingMode::kNone);
	for (const auto& stage : stages_) {
		infos_[stage->modelIndex].model->Draw(stage->worldTransform, camera);
	}
	Model::PostDraw();
}

bool StageManager::TryGetGroundZ(float y, float& outZ) const {
	for (const auto& stage : stages_) {
		if (y < stage->minY || y > stage->maxY) {
			continue;
		}
		const ModelInfo& m = infos_[stage->modelIndex];
		const int n = static_cast<int>(m.top.size());
		// ワールドY座標から対応するローカルXインデックスを算出
		float localX = m.maxX - (y - stage->minY) / kStageScale;
		float f = (localX - m.minX) / kProfileStep;
		int i = (std::min)((std::max)(static_cast<int>(f), 0), n - 2);
		float t = (std::min)((std::max)(f - i, 0.0f), 1.0f);
		float a = m.top[i];
		float b = m.top[i + 1];
		if (std::isnan(a) || std::isnan(b)) {
			return false; // 足場の穴
		}
		outZ = stage->worldTransform.translation_.z - (a + (b - a) * t) * kStageScale;
		return true;
	}
	return false;
}

Vector3 StageManager::GetRespawnPosition(float y) const {
	assert(!stages_.empty());
	const Stage* target = stages_.front().get();
	for (const auto& stage : stages_) {
		if (stage->minY <= y) {
			target = stage.get();
		}
	}
	float ry = target->minY + 3.0f;
	float ground = kGroundZ;
	TryGetGroundZ(ry, ground);
	return {0.0f, ry, ground - kPlayerHalfHeight};
}