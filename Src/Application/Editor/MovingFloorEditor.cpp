#include "../../Pch.h"
#include "MovingFloorEditor.h"
#include "../Manager/StageManager.h"
#include <fstream>
#include <sstream>

//----------------------------------------------------------
// DrawGui
//----------------------------------------------------------
void MovingFloorEditor::DrawGui()
{
	if (!ImGui::Begin(U8("移動床 エディタ")))
	{
		ImGui::End();
		return;
	}

	// ── 追加ボタン ──────────────────────────────────────────
	if (ImGui::Button(U8("床を追加")))
	{
		MovingFloorData d;
		m_floors.push_back(d);
		m_selectedIndex = static_cast<int>(m_floors.size()) - 1;
		m_dirty = true;
	}
	ImGui::SameLine();
	if (ImGui::Button(U8("保存"))) { Save(); }
	ImGui::SameLine();
	if (ImGui::Button(U8("再読込"))) { Load(); }

	ImGui::Separator();

	// ── リスト ──────────────────────────────────────────────
	ImGui::Text(U8("床一覧"));
	const char* axisNames[] = { "X", "Y", "Z" };
	for (int i = 0; i < static_cast<int>(m_floors.size()); ++i)
	{
		const auto& f = m_floors[i];
		const char* axName = axisNames[static_cast<int>(f.axis)];
		char label[80];
		std::snprintf(label, sizeof(label), "[%d] Axis:%s  (%.1f, %.1f, %.1f)##%d",
			i, axName, f.center.x, f.center.y, f.center.z, i);

		const bool selected = (m_selectedIndex == i);
		if (ImGui::Selectable(label, selected))
		{
			m_selectedIndex = i;
		}
	}

	ImGui::Separator();

	// ── インスペクター ───────────────────────────────────────
	if (m_selectedIndex >= 0 && m_selectedIndex < static_cast<int>(m_floors.size()))
	{
		auto& f = m_floors[m_selectedIndex];

		ImGui::Text(U8("インスペクタ [%d]"), m_selectedIndex);

		// 中心位置
		float center[3] = { f.center.x, f.center.y, f.center.z };
		if (ImGui::DragFloat3(U8("中心"), center, 0.1f))
		{
			f.center = { center[0], center[1], center[2] };
			m_dirty  = true;
		}

		// 移動軸
		int axisIdx = static_cast<int>(f.axis);
		if (ImGui::Combo(U8("軸"), &axisIdx, axisNames, 3))
		{
			f.axis  = static_cast<MovingFloorData::Axis>(axisIdx);
			m_dirty = true;
		}

		// 移動距離（片道）
		if (ImGui::DragFloat(U8("可動範囲"), &f.range, 0.1f, 0.1f, 100.0f))
		{
			m_dirty = true;
		}

		// 移動速度
		if (ImGui::DragFloat(U8("速さ"), &f.speed, 0.1f, 0.1f, 50.0f))
		{
			m_dirty = true;
		}

		// 折り返し一時停止
		if (ImGui::DragFloat(U8("待ち時間(秒)"), &f.waitTime, 0.05f, 0.0f, 10.0f))
		{
			m_dirty = true;
		}

		// 開始位置
		{
			const bool isY = (f.axis == MovingFloorData::Axis::Y);
			const char* label0 = isY ? "Bottom" : "Left";
			const char* label2 = isY ? "Top"    : "Right";
			const char* phaseNames[] = { label0, U8("中央"), label2 };
			// StartPhase: NegEnd=-1, Center=0, PosEnd=1 → combo index: 0,1,2
			int phaseIdx = static_cast<int>(f.startPhase) + 1;
			if (ImGui::Combo(U8("開始位置"), &phaseIdx, phaseNames, 3))
			{
				f.startPhase = static_cast<MovingFloorData::StartPhase>(phaseIdx - 1);
				m_dirty = true;
			}
		}

		// サイズ
		float size[3] = { f.size.x, f.size.y, f.size.z };
		if (ImGui::DragFloat3(U8("サイズ"), size, 0.05f, 0.1f, 50.0f))
		{
			f.size  = { size[0], size[1], size[2] };
			m_dirty = true;
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Text(U8("重力フェイス設定"));

		if (ImGui::Checkbox(U8("通常重力（下）"), &f.bNormalGravity)) { m_dirty = true; }
		if (f.bNormalGravity)
		{
			ImGui::SameLine();
			ImGui::TextColored({ 1.0f, 0.6f, 0.1f, 1.0f }, "<- 通常地面");
		}
		else
		{
			const char* modeNames[] = { U8("内向き"), U8("外向き"), U8("継承"), U8("下"), U8("上"), U8("左"), U8("右") };

			int topMode = static_cast<int>(f.faceTop);
			if (ImGui::Combo(U8("上面"),    &topMode, modeNames, IM_ARRAYSIZE(modeNames)))
			{ f.faceTop    = static_cast<BoxFaceGravityMode>(topMode);    m_dirty = true; }

			int bottomMode = static_cast<int>(f.faceBottom);
			if (ImGui::Combo(U8("下面"), &bottomMode, modeNames, IM_ARRAYSIZE(modeNames)))
			{ f.faceBottom = static_cast<BoxFaceGravityMode>(bottomMode); m_dirty = true; }

			int leftMode = static_cast<int>(f.faceLeft);
			if (ImGui::Combo(U8("左面"),   &leftMode, modeNames, IM_ARRAYSIZE(modeNames)))
			{ f.faceLeft   = static_cast<BoxFaceGravityMode>(leftMode);   m_dirty = true; }

			int rightMode = static_cast<int>(f.faceRight);
			if (ImGui::Combo(U8("右面"),  &rightMode, modeNames, IM_ARRAYSIZE(modeNames)))
			{ f.faceRight  = static_cast<BoxFaceGravityMode>(rightMode);  m_dirty = true; }
		}

		ImGui::Spacing();

		// 削除ボタン
		if (ImGui::Button(U8("削除")))
		{
			m_floors.erase(m_floors.begin() + m_selectedIndex);
			m_selectedIndex = -1;
			m_dirty = true;
		}
	}

	ImGui::End();
}

//----------------------------------------------------------
// DrawDebug  ─ ワイヤーフレームボックスで配置を可視化
//----------------------------------------------------------
void MovingFloorEditor::DrawDebug() const
{
	for (const auto& f : m_floors)
	{
		// 軸方向ベクトル
		Math::Vector3 dir = Math::Vector3::Zero;
		if      (f.axis == MovingFloorData::Axis::X) { dir = { 1.0f, 0.0f, 0.0f }; }
		else if (f.axis == MovingFloorData::Axis::Y) { dir = { 0.0f, 1.0f, 0.0f }; }
		else                                          { dir = { 0.0f, 0.0f, 1.0f }; }

		// 移動範囲の両端を線で表示
		const Math::Vector3 posA = f.center + dir * f.range;
		const Math::Vector3 posB = f.center - dir * f.range;

		KdDebugWireFrame wire;
		wire.AddDebugLine(posA, posB, { 1.0f, 0.7f, 0.0f, 1.0f });
		wire.AddDebugBox(Math::Matrix::CreateTranslation(f.center), f.size,
			Math::Vector3::Zero, false, { 1.0f, 0.7f, 0.0f, 1.0f });
		wire.Draw();
	}
}

//----------------------------------------------------------
// Save  ─ CSV へ書き出し
// フォーマット: cx, cy, cz, axis, range, sx, sy, sz
//----------------------------------------------------------
void MovingFloorEditor::Save() const
{
	std::ofstream ofs(StageManager::Instance().ResolvePath("moving_floors.csv"));
	if (!ofs) { return; }

	for (const auto& f : m_floors)
	{
		ofs << f.center.x << ","
			<< f.center.y << ","
			<< f.center.z << ","
			<< static_cast<int>(f.axis) << ","
			<< f.range    << ","
			<< f.size.x   << ","
			<< f.size.y   << ","
			<< f.size.z   << ","
				<< f.speed    << ","
				<< static_cast<int>(f.startPhase) << ","
				<< (f.bNormalGravity ? 1 : 0) << ","
			<< static_cast<int>(f.faceTop)    << ","
			<< static_cast<int>(f.faceBottom) << ","
			<< static_cast<int>(f.faceLeft)   << ","
			<< static_cast<int>(f.faceRight)  << ","
			<< f.waitTime << "\n";
	}
}

//----------------------------------------------------------
// Load  ─ CSV から読み込み
//----------------------------------------------------------
void MovingFloorEditor::Load()
{
	m_floors.clear();
	m_selectedIndex = -1;

	std::ifstream ifs(StageManager::Instance().ResolvePath("moving_floors.csv"));
	if (!ifs) { return; }

	std::string line;
	while (std::getline(ifs, line))
	{
		if (line.empty()) { continue; }

		std::istringstream ss(line);
		std::string token;
		std::vector<std::string> tokens;
		while (std::getline(ss, token, ',')) { tokens.push_back(token); }
		if (tokens.size() < 8) { continue; }

		MovingFloorData d;
		d.center = { std::stof(tokens[0]), std::stof(tokens[1]), std::stof(tokens[2]) };
		d.axis   = static_cast<MovingFloorData::Axis>(std::stoi(tokens[3]));
		d.range  = std::stof(tokens[4]);
		d.size   = { std::stof(tokens[5]), std::stof(tokens[6]), std::stof(tokens[7]) };

		// 後方互換：古いCSVにはフィールドがない場合はデフォルト値を使用
		if (tokens.size() >= 9)  { d.speed = std::stof(tokens[8]); }
		if (tokens.size() >= 10) { d.startPhase = static_cast<MovingFloorData::StartPhase>(std::stoi(tokens[9])); }
		if (tokens.size() >= 15)
		{
			d.bNormalGravity = (std::stoi(tokens[10]) != 0);
			d.faceTop    = static_cast<BoxFaceGravityMode>(std::stoi(tokens[11]));
			d.faceBottom = static_cast<BoxFaceGravityMode>(std::stoi(tokens[12]));
			d.faceLeft   = static_cast<BoxFaceGravityMode>(std::stoi(tokens[13]));
			d.faceRight  = static_cast<BoxFaceGravityMode>(std::stoi(tokens[14]));
		}
		if (tokens.size() >= 16) { d.waitTime = std::stof(tokens[15]); }

		m_floors.push_back(d);
	}
}
