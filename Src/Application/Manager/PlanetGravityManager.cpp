#include "../../Pch.h"
#include "PlanetGravityManager.h"
#include "../Manager/ModelManager.h"
#include <fstream>
#include <sstream>

//----------------------------------------------------------
// PlanetData::InitModel
// Planet.gltf を読み込み、COL ノードをコライダーに登録する
//----------------------------------------------------------
void PlanetData::InitModel()
{
	const char* path = (Shape == PlanetShape::Box)
		? PlanetConst::BoxModelPath
		: PlanetConst::ModelPath;

	const auto spData = ModelManager::Instance().GetModel(path);
	if (!spData) { return; }

	modelWork = std::make_shared<KdModelWork>();
	modelWork->SetModelData(spData);
	UpdateWorld();
	modelWork->CalcNodeMatrices();

	// コライダー登録
	pCollider = std::make_unique<KdCollider>();

	const auto& colIndices  = spData->GetCollisionMeshNodeIndices();
	const auto& meshIndices = spData->GetMeshNodeIndices();
	const auto& useIndices  = colIndices.empty() ? meshIndices : colIndices;

	for (int idx : useIndices)
	{
		const std::string& name = spData->GetOriginalNodes()[idx].m_name;
		auto spShape = std::make_unique<KdModelCollision>(
			modelWork,
			KdCollider::TypeGround | KdCollider::TypeBump);
		spShape->SetNodeFilter({ idx });
		pCollider->RegisterCollisionShape(name, std::move(spShape));
	}
}

//----------------------------------------------------------
// PlanetData::UpdateWorld
// Position に合わせてモデルのワールド行列を更新する
//----------------------------------------------------------
void PlanetData::UpdateWorld()
{
	const Math::Matrix scale = (Shape == PlanetShape::Box)
		? Math::Matrix::CreateScale(BoxHalfExtents)
		: Math::Matrix::CreateScale(SurfaceRadius);
	const Math::Matrix translate = Math::Matrix::CreateTranslation(Position);
	mWorld = scale * translate;
	if (modelWork) { modelWork->CalcNodeMatrices(); }
}


void PlanetGravityManager::DrawGui()
{
	if (!ImGui::Begin("Planet Gravity"))
	{
		ImGui::End();
		return;
	}

	// エディタGUIを開いている間は編集追従のため毎フレーム行列を更新する
	// （通常プレイ時はGUIを閉じているので負荷はかからない）
	MarkWorldDirty();

	if (ImGui::Button("Add Planet"))
	{
		PlanetData newPlanet{};
		newPlanet.InitModel();
		m_planets.push_back(std::move(newPlanet));
		m_selectedIndex = static_cast<int>(m_planets.size()) - 1;
	}

	ImGui::Separator();
	ImGui::Text("Planet List");

	for (int i = 0; i < static_cast<int>(m_planets.size()); ++i)
	{
		const auto& p = m_planets[i];
		char label[80];
		std::snprintf(label, sizeof(label), "[%d]%s (%.1f, %.1f, %.1f) R=%.1f##%d",
			i, p.bNormalGravity ? "[N]" : "",
			p.Position.x, p.Position.y, p.Position.z, p.SurfaceRadius, i);

		if (ImGui::Selectable(label, m_selectedIndex == i))
		{
			m_selectedIndex = i;
		}
	}

	ImGui::Separator();

	if (m_selectedIndex >= 0 && m_selectedIndex < static_cast<int>(m_planets.size()))
	{
		auto& p = m_planets[m_selectedIndex];
		ImGui::Text("Inspector");

		float pos[3] = { p.Position.x, p.Position.y, p.Position.z };
		if (ImGui::DragFloat3("Position", pos, 0.1f))
		{
			p.Position = { pos[0], pos[1], pos[2] };
		}

		ImGui::DragFloat("Surface Radius",  &p.SurfaceRadius, 0.1f, 0.5f,  1000.0f);
		ImGui::DragFloat("Ground Radius",   &p.GroundRadius,  0.1f, 0.5f,  1000.0f);
		ImGui::DragFloat("Gravity Radius",  &p.GravityRadius, 0.1f, 1.0f,  2000.0f);
		ImGui::DragFloat("Gravity Strength", &p.GravityStrength, 0.01f, 0.1f, 10.0f);
		ImGui::DragInt  ("Priority",        &p.Priority,      1,    0,     100);
		ImGui::Checkbox ("Normal Gravity (Down)", &p.bNormalGravity);
		if (p.bNormalGravity)
		{
			ImGui::SameLine();
			ImGui::TextColored({ 1.0f, 0.6f, 0.1f, 1.0f }, "<- 通常地面ゾーン");
		}

		// Shape 選択
		{
			const char* shapeNames[] = { "Sphere", "Box" };
			int shapeIdx = static_cast<int>(p.Shape);
			if (ImGui::Combo("Shape", &shapeIdx, shapeNames, IM_ARRAYSIZE(shapeNames)))
			{
				p.Shape = static_cast<PlanetShape>(shapeIdx);
				p.InitModel();  // モデルを切り替え
			}
		}
		if (p.Shape == PlanetShape::Box)
		{
			float half[3] = { p.BoxHalfExtents.x, p.BoxHalfExtents.y, p.BoxHalfExtents.z };
			if (ImGui::DragFloat3("Box Half Extents", half, 0.1f, 0.1f, 1000.0f))
			{
				p.BoxHalfExtents = { half[0], half[1], half[2] };
			}

			// Box各面の重力モード設定
			ImGui::Separator();
			ImGui::Text("Box Face Gravity Modes:");
			const char* modeNames[] = { "Inward", "Outward", "Inherit", "Down", "Up", "Left", "Right" };

			int topMode = static_cast<int>(p.BoxFaceGravityTop);
			if (ImGui::Combo("Top Face", &topMode, modeNames, IM_ARRAYSIZE(modeNames)))
			{
				p.BoxFaceGravityTop = static_cast<BoxFaceGravityMode>(topMode);
			}

			int bottomMode = static_cast<int>(p.BoxFaceGravityBottom);
			if (ImGui::Combo("Bottom Face", &bottomMode, modeNames, IM_ARRAYSIZE(modeNames)))
			{
				p.BoxFaceGravityBottom = static_cast<BoxFaceGravityMode>(bottomMode);
			}

			int leftMode = static_cast<int>(p.BoxFaceGravityLeft);
			if (ImGui::Combo("Left Face", &leftMode, modeNames, IM_ARRAYSIZE(modeNames)))
			{
				p.BoxFaceGravityLeft = static_cast<BoxFaceGravityMode>(leftMode);
			}

			int rightMode = static_cast<int>(p.BoxFaceGravityRight);
			if (ImGui::Combo("Right Face", &rightMode, modeNames, IM_ARRAYSIZE(modeNames)))
			{
				p.BoxFaceGravityRight = static_cast<BoxFaceGravityMode>(rightMode);
			}
		}

		ImGui::TextDisabled("Surface <= Ground <= Gravity  |  Priority: high = win");

		if (ImGui::Button("Delete"))
		{
			m_planets.erase(m_planets.begin() + m_selectedIndex);
			m_selectedIndex = -1;
		}
	}

	ImGui::Separator();
	if (ImGui::Button("Save")) { Save(); }
	ImGui::SameLine();
	if (ImGui::Button("Load")) { Load(); }

	ImGui::End();
}

void PlanetGravityManager::DrawDebugShapes() const
{
	// XY平面の円を線分で近似して描く（2.5D用）
	constexpr int   Segments  = 32;
	constexpr float TwoPi     = DirectX::XM_2PI;

	for (int i = 0; i < static_cast<int>(m_planets.size()); ++i)
	{
		const auto& p       = m_planets[i];
		const bool  sel     = (i == m_selectedIndex);
		const float z       = p.Position.z;

		// ---- ヘルパー：半径 r の円を color で描く ----
		auto drawCircle = [&](float r, const Math::Color& color)
		{
			KdDebugWireFrame wire;
			for (int s = 0; s < Segments; ++s)
			{
				const float a0 = TwoPi * s       / Segments;
				const float a1 = TwoPi * (s + 1) / Segments;
				const Math::Vector3 p0 = { p.Position.x + r * std::cosf(a0),
										   p.Position.y + r * std::sinf(a0), z };
				const Math::Vector3 p1 = { p.Position.x + r * std::cosf(a1),
										   p.Position.y + r * std::sinf(a1), z };
				wire.AddDebugLine(p0, p1, color);
			}
			wire.Draw();
		};

		const Math::Color surfaceColor = p.bNormalGravity
			? Math::Color(1.0f, 0.5f, 0.0f, 1.0f)
			: (sel
				? Math::Color(1.0f, 1.0f, 0.0f, 1.0f)
				: Math::Color(PlanetConst::SurfaceColorR, PlanetConst::SurfaceColorG,
							  PlanetConst::SurfaceColorB, PlanetConst::SurfaceColorA));

		if (p.Shape == PlanetShape::Box)
		{
			// ---- Box: AABB をワイヤーで描く ----
			auto drawBox = [&](const Math::Vector3& half, const Math::Color& color)
			{
				const float minX = p.Position.x - half.x;
				const float maxX = p.Position.x + half.x;
				const float minY = p.Position.y - half.y;
				const float maxY = p.Position.y + half.y;
				KdDebugWireFrame wire;
				wire.AddDebugLine({ minX, minY, z }, { maxX, minY, z }, color);
				wire.AddDebugLine({ maxX, minY, z }, { maxX, maxY, z }, color);
				wire.AddDebugLine({ maxX, maxY, z }, { minX, maxY, z }, color);
				wire.AddDebugLine({ minX, maxY, z }, { minX, minY, z }, color);
				wire.Draw();
			};

			// Surface（BoxHalfExtents で描画）
			drawBox(p.BoxHalfExtents, surfaceColor);
			// Ground（BoxHalfExtents + GroundRadius マージン）
			const Math::Vector3 groundHalf = p.BoxHalfExtents + Math::Vector3(p.GroundRadius - p.SurfaceRadius);
			drawBox(groundHalf, Math::Color(0.2f, 1.0f, 0.3f, 0.8f));
			// Gravity（AABB 外接円として円で描く）
			drawCircle(p.GravityRadius,
				Math::Color(PlanetConst::GravityColorR, PlanetConst::GravityColorG,
							PlanetConst::GravityColorB, PlanetConst::GravityColorA));
		}
		else
		{
			// ---- Sphere: 既存の円描画 ----
			drawCircle(p.SurfaceRadius, surfaceColor);
			drawCircle(p.GroundRadius,  Math::Color(0.2f, 1.0f, 0.3f, 0.8f));
			drawCircle(p.GravityRadius,
				Math::Color(PlanetConst::GravityColorR, PlanetConst::GravityColorG,
							PlanetConst::GravityColorB, PlanetConst::GravityColorA));
		}
	}
}

const PlanetData* PlanetGravityManager::FindNearestPlanet(const Math::Vector3& _charPos) const
{
	return GetPlanet(FindNearestPlanetIndex(_charPos));
}

GravityInfluenceResult PlanetGravityManager::ComputeGravityInfluence(const Math::Vector3& _charPos,
	const Math::Vector3& currentGravDir) const
{
	GravityInfluenceResult result;
	Math::Vector3 totalGravity = { 0.0f, 0.0f, 0.0f };
	float maxInfluence = 0.0f;

	for (int i = 0; i < static_cast<int>(m_planets.size()); ++i)
	{
		const auto& p = m_planets[i];
		const float dx = _charPos.x - p.Position.x;
		const float dy = _charPos.y - p.Position.y;

		// 惑星までの距離と方向を計算
		float dist = 0.0f;
		Math::Vector3 gravDir = { 0.0f, -1.0f, 0.0f };
		Math::Vector3 upDir   = { 0.0f,  1.0f, 0.0f };

		if (p.Shape == PlanetShape::Box)
		{
			const Math::Vector3 lp = _charPos - p.Position;
			const Math::Vector3& half = p.BoxHalfExtents;

			// Boxの表面からの符号付き距離を計算
			const float ox = std::max(0.0f, std::abs(lp.x) - half.x);
			const float oy = std::max(0.0f, std::abs(lp.y) - half.y);
			const float surfaceDist = std::sqrtf(ox * ox + oy * oy);

			// Box表面からGravityRadius外なら影響なし
			if (surfaceDist >= p.GravityRadius) { continue; }

			// 最近傍面を判定
			const float penX = half.x - std::abs(lp.x);
			const float penY = half.y - std::abs(lp.y);

			// どの面に最も近いか判定
			BoxFaceGravityMode faceMode = BoxFaceGravityMode::Inward;
			Math::Vector3 faceOutDir = { 0.0f, 1.0f, 0.0f };

			if (penX < penY)
			{
				// 左右面が最近傍
				if (lp.x >= 0.0f)
				{
					faceMode = p.BoxFaceGravityRight;
					faceOutDir = { 1.0f, 0.0f, 0.0f };
				}
				else
				{
					faceMode = p.BoxFaceGravityLeft;
					faceOutDir = { -1.0f, 0.0f, 0.0f };
				}
			}
			else
			{
				// 上下面が最近傍
				if (lp.y >= 0.0f)
				{
					faceMode = p.BoxFaceGravityTop;
					faceOutDir = { 0.0f, 1.0f, 0.0f };
				}
				else
				{
					faceMode = p.BoxFaceGravityBottom;
					faceOutDir = { 0.0f, -1.0f, 0.0f };
				}
			}

			// 面のモードに応じて重力方向を決定
			if (p.bNormalGravity)
			{
				gravDir = { 0.0f, -1.0f, 0.0f };
				upDir   = { 0.0f,  1.0f, 0.0f };
			}
			else
			{
				switch (faceMode)
				{
				case BoxFaceGravityMode::Inward:
					upDir   = faceOutDir;
					gravDir = -upDir;
					break;
				case BoxFaceGravityMode::Outward:
					gravDir = faceOutDir;
					upDir   = -faceOutDir;
					break;
				case BoxFaceGravityMode::Inherit:
					// 現在の重力方向をそのまま継承
					gravDir = currentGravDir;
					upDir   = -currentGravDir;
					break;
				case BoxFaceGravityMode::Down:
					gravDir = { 0.0f, -1.0f, 0.0f };
					upDir   = { 0.0f,  1.0f, 0.0f };
					break;
				case BoxFaceGravityMode::Up:
					gravDir = { 0.0f, 1.0f, 0.0f };
					upDir   = { 0.0f, -1.0f, 0.0f };
					break;
				case BoxFaceGravityMode::Left:
					gravDir = { -1.0f, 0.0f, 0.0f };
					upDir   = { 1.0f, 0.0f, 0.0f };
					break;
				case BoxFaceGravityMode::Right:
					gravDir = { 1.0f, 0.0f, 0.0f };
					upDir   = { -1.0f, 0.0f, 0.0f };
					break;
				}
			}

			// 表面距離を使用（内部なら小さな値で統一）
			dist = std::max(surfaceDist, 0.5f);
		}
		else // Sphere
		{
			const float centerDist = std::sqrtf(dx * dx + dy * dy);

			// Sphere表面からの距離（中心距離 - 表面半径）
			const float surfaceDist = std::max(0.0f, centerDist - p.SurfaceRadius);

			if (surfaceDist >= p.GravityRadius) { continue; }

			if (p.bNormalGravity)
			{
				gravDir = { 0.0f, -1.0f, 0.0f };
				upDir   = { 0.0f,  1.0f, 0.0f };
			}
			else
			{
				if (centerDist > 0.001f)
				{
					upDir = { dx / centerDist, dy / centerDist, 0.0f };
					gravDir = -upDir;
				}
			}

			// 表面距離を使用（内部なら小さな値）
			dist = std::max(surfaceDist, 0.5f);
		}

		// 影響力計算：距離の2乗に反比例 × 惑星固有の重力強度
		const float rawInfluence = (PlanetConst::GravityInfluenceStrength * p.GravityStrength) /
								   (dist * dist + PlanetConst::GravityInfluenceEpsilon);

		// 重力圏の外縁に近づくほど重力を 0 にフェードアウト（ふわ〜と離れていく感）
		// t = 1.0（表面付近）→ t = 0.0（重力圏の端）のスムーズステップ
		const float t = 1.0f - std::min(dist / p.GravityRadius, 1.0f);
		const float falloff = t * t * (3.0f - 2.0f * t);  // smoothstep
		const float influence = rawInfluence * falloff;

		// 重力ベクトルを加算
		totalGravity += gravDir * influence;

		// 最も影響力が強い惑星を記録
		if (influence > maxInfluence)
		{
			maxInfluence = influence;
			result.dominantUpDir = upDir;
			result.dominantPlanetIdx = i;
		}
	}

	// 合成重力を正規化
	const float totalLen = totalGravity.Length();
	if (totalLen > 0.001f)
	{
		result.totalGravityDir = totalGravity / totalLen;
		result.hasInfluence = true;
	}
	else
	{
		result.totalGravityDir = { 0.0f, -1.0f, 0.0f };
		result.dominantUpDir   = { 0.0f,  1.0f, 0.0f };
		result.hasInfluence = false;
	}

	return result;
}

int PlanetGravityManager::FindNearestPlanetIndex(const Math::Vector3& _charPos) const
{
	int   bestIdx  = -1;
	float bestDist = FLT_MAX;
	int   bestPri  = INT_MIN;

	for (int i = 0; i < static_cast<int>(m_planets.size()); ++i)
	{
		const auto& p = m_planets[i];
		const float dx = _charPos.x - p.Position.x;
		const float dy = _charPos.y - p.Position.y;

		float dist = 0.0f;
		bool  inRange = false;

		if (p.Shape == PlanetShape::Box)
		{
			// AABB 外接円で粗判定してから距離を計算
			const float roughRadius = std::max({ p.BoxHalfExtents.x, p.BoxHalfExtents.y }) + PlanetConst::GravityRadiusMargin;
			const float roughDist   = std::sqrtf(dx * dx + dy * dy);
			if (roughDist < roughRadius)
			{
				// AABB 外部点からの最短距離
				const float ox = std::max(0.0f, std::abs(dx) - p.BoxHalfExtents.x);
				const float oy = std::max(0.0f, std::abs(dy) - p.BoxHalfExtents.y);

				if (ox == 0.0f && oy == 0.0f)
				{
					// Box内部 → 中心からの距離を使用（Sphere惑星と公平に比較）
					dist = roughDist;
				}
				else
				{
					// Box外部 → 外部点からの最短距離
					dist = std::sqrtf(ox * ox + oy * oy);
				}

				inRange = (dist < p.GravityRadius);
			}
		}
		else
		{
			dist    = std::sqrtf(dx * dx + dy * dy);
			inRange = (dist < p.GravityRadius);
		}

		if (!inRange) { continue; }

		if (p.Priority > bestPri || (p.Priority == bestPri && dist < bestDist))
		{
			bestPri  = p.Priority;
			bestDist = dist;
			bestIdx  = i;
		}
	}

	return bestIdx;
}

void PlanetGravityManager::Save() const
{
	std::ofstream ofs(PlanetConst::SavePath);
	if (!ofs) { return; }

	for (const auto& p : m_planets)
	{
		ofs << p.Position.x << ","
			<< p.Position.y << ","
			<< p.Position.z << ","
			<< p.SurfaceRadius << ","
			<< p.GroundRadius  << ","
			<< p.GravityRadius << ","
			<< p.Priority      << ","
			<< (p.bNormalGravity ? 1 : 0) << ","
			<< p.GravityStrength << ","
			<< static_cast<int>(p.Shape) << ","
			<< p.BoxHalfExtents.x << ","
			<< p.BoxHalfExtents.y << ","
			<< p.BoxHalfExtents.z << ","
			<< static_cast<int>(p.BoxFaceGravityTop) << ","
			<< static_cast<int>(p.BoxFaceGravityBottom) << ","
			<< static_cast<int>(p.BoxFaceGravityLeft) << ","
			<< static_cast<int>(p.BoxFaceGravityRight) << "\n";
	}
}

void PlanetGravityManager::Load()
{
	std::ifstream ifs(PlanetConst::SavePath);
	if (!ifs) { return; }

	m_planets.clear();

	std::string line;
	while (std::getline(ifs, line))
	{
		if (line.empty()) { continue; }

		std::istringstream ss(line);
		std::string token;
		std::vector<std::string> tokens;
		while (std::getline(ss, token, ',')) { tokens.push_back(token); }
		if (tokens.size() < 5) { continue; }

		PlanetData p;
		p.Position.x    = std::stof(tokens[0]);
		p.Position.y    = std::stof(tokens[1]);
		p.Position.z    = std::stof(tokens[2]);
		p.SurfaceRadius = std::stof(tokens[3]);
		if (tokens.size() >= 17)
		{
			// 最新フォーマット：BoxFaceGravityMode を含む
			p.GroundRadius   = std::stof(tokens[4]);
			p.GravityRadius  = std::stof(tokens[5]);
			p.Priority       = std::stoi(tokens[6]);
			p.bNormalGravity = (std::stoi(tokens[7]) != 0);
			p.GravityStrength = std::stof(tokens[8]);
			p.Shape          = static_cast<PlanetShape>(std::stoi(tokens[9]));
			p.BoxHalfExtents = { std::stof(tokens[10]), std::stof(tokens[11]), std::stof(tokens[12]) };
			p.BoxFaceGravityTop    = static_cast<BoxFaceGravityMode>(std::stoi(tokens[13]));
			p.BoxFaceGravityBottom = static_cast<BoxFaceGravityMode>(std::stoi(tokens[14]));
			p.BoxFaceGravityLeft   = static_cast<BoxFaceGravityMode>(std::stoi(tokens[15]));
			p.BoxFaceGravityRight  = static_cast<BoxFaceGravityMode>(std::stoi(tokens[16]));
		}
		else if (tokens.size() >= 13)
		{
			// 旧フォーマット：GravityStrength を含む、BoxFaceGravityMode なし
			p.GroundRadius   = std::stof(tokens[4]);
			p.GravityRadius  = std::stof(tokens[5]);
			p.Priority       = std::stoi(tokens[6]);
			p.bNormalGravity = (std::stoi(tokens[7]) != 0);
			p.GravityStrength = std::stof(tokens[8]);
			p.Shape          = static_cast<PlanetShape>(std::stoi(tokens[9]));
			p.BoxHalfExtents = { std::stof(tokens[10]), std::stof(tokens[11]), std::stof(tokens[12]) };
		}
		else if (tokens.size() >= 12)
		{
			// 旧フォーマット：GravityStrength なし
			p.GroundRadius   = std::stof(tokens[4]);
			p.GravityRadius  = std::stof(tokens[5]);
			p.Priority       = std::stoi(tokens[6]);
			p.bNormalGravity = (std::stoi(tokens[7]) != 0);
			p.Shape          = static_cast<PlanetShape>(std::stoi(tokens[8]));
			p.BoxHalfExtents = { std::stof(tokens[9]), std::stof(tokens[10]), std::stof(tokens[11]) };
		}
		else if (tokens.size() >= 8)
		{
			p.GroundRadius   = std::stof(tokens[4]);
			p.GravityRadius  = std::stof(tokens[5]);
			p.Priority       = std::stoi(tokens[6]);
			p.bNormalGravity = (std::stoi(tokens[7]) != 0);
		}
		else if (tokens.size() >= 7)
		{
			p.GroundRadius  = std::stof(tokens[4]);
			p.GravityRadius = std::stof(tokens[5]);
			p.Priority      = std::stoi(tokens[6]);
		}
		else if (tokens.size() >= 6)
		{
			p.GroundRadius  = std::stof(tokens[4]);
			p.GravityRadius = std::stof(tokens[5]);
		}
		else
		{
			p.GravityRadius = std::stof(tokens[4]);
			p.GroundRadius  = p.GravityRadius;
		}
		p.InitModel();
		m_planets.push_back(std::move(p));
	}

	// 読み込んだ惑星のワールド行列を次の PostUpdate で再計算させる
	m_worldDirty = true;
}

void PlanetGravityManager::PostUpdate()
{
	// 惑星は普段動かないので、変更があった時だけワールド行列を再計算する。
	// CalcNodeMatrices() は重いので毎フレーム実行しない。
	if (!m_worldDirty) { return; }

	for (auto& p : m_planets)
	{
		p.UpdateWorld();
	}
	m_worldDirty = false;
}

void PlanetGravityManager::DrawLit() const
{
	auto& shader = KdShaderManager::Instance().m_StandardShader;

	for (const auto& p : m_planets)
	{
		if (!p.modelWork || !p.modelWork->GetData()) { continue; }

		const bool useTriplanar = (p.Shape == PlanetShape::Box);
		if (useTriplanar)
		{
			shader.SetTriplanarUV(true, PlanetConst::TriplanarScale);
		}
		else
		{
			// 球惑星：法線を中心方向で解析計算してローポリの段差を消す
			shader.SetSphereNormal(true);
		}

		shader.DrawModel(*p.modelWork, p.mWorld);

		if (useTriplanar)
		{
			shader.SetTriplanarUV(false);
		}
		else
		{
			shader.SetSphereNormal(false);
		}
	}
}
