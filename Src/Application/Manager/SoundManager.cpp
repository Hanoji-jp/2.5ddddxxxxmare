#include "../main.h"
#include "SoundManager.h"
#include "../Const/SoundConst.h"
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <sstream>

namespace
{
	// ネイティブ(CP932)文字列 → UTF-8（ImGui表示用）
	std::string AcpToUtf8(const std::string& acp)
	{
		if (acp.empty()) { return {}; }
		const int wlen = MultiByteToWideChar(CP_ACP, 0, acp.c_str(), -1, nullptr, 0);
		if (wlen <= 0) { return acp; }
		std::wstring w(wlen, L'\0');
		MultiByteToWideChar(CP_ACP, 0, acp.c_str(), -1, w.data(), wlen);
		const int u8len = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
		if (u8len <= 0) { return acp; }
		std::string u8(u8len, '\0');
		WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, u8.data(), u8len, nullptr, nullptr);
		if (!u8.empty() && u8.back() == '\0') { u8.pop_back(); }
		return u8;
	}

	// パスからファイル名だけ取り出す（表示用）
	std::string BaseNameOf(const std::string& path)
	{
		const size_t s = path.find_last_of("/\\");
		return (s == std::string::npos) ? path : path.substr(s + 1);
	}
}

bool SoundManager::Exists(std::string_view path)
{
	if (path.empty()) { return false; }
	std::error_code ec;
	return std::filesystem::exists(std::filesystem::path(std::string(path)), ec);
}

// "Asset/Sound/BGM/Game.mp3" → "Asset/Sound/BGM/Game_muffled.mp3"
std::string SoundManager::MuffledOf(std::string_view basePath)
{
	std::string p(basePath);
	const size_t dot = p.find_last_of('.');
	if (dot == std::string::npos) { return p + SoundConst::MuffleSuffix; }
	return p.substr(0, dot) + SoundConst::MuffleSuffix + p.substr(dot);
}

void SoundManager::Preload(std::string_view path)
{
	if (!Exists(path)) { return; }
	// KdAudioManager に読み込ませてキャッシュさせる（再生はしない）
	std::initializer_list<std::string_view> il = { path };
	KdAudioManager::Instance().LoadSoundAssets(il);
}

void SoundManager::PlaySE(std::string_view path, float vol)
{
	if (!Exists(path)) { return; }   // 未配置なら無音
	auto inst = KdAudioManager::Instance().Play(path, false);
	if (inst) { inst->SetVolume(vol * m_masterVol * m_seUserVol); TrackSe(inst); }
}

void SoundManager::PlaySE3D(std::string_view path, const Math::Vector3& pos, float vol)
{
	if (!Exists(path)) { return; }
	auto inst = KdAudioManager::Instance().Play3D(path, pos, false);
	if (inst) { inst->SetVolume(vol * m_masterVol * m_seUserVol); TrackSe(inst); }
}

void SoundManager::TrackSe(const std::shared_ptr<KdSoundInstance>& inst)
{
	// 期限切れ（再生終了で解放済み）を掃除しつつ追加
	m_activeSe.erase(
		std::remove_if(m_activeSe.begin(), m_activeSe.end(),
			[](const std::weak_ptr<KdSoundInstance>& w) { return w.expired(); }),
		m_activeSe.end());
	m_activeSe.push_back(inst);
}

void SoundManager::PauseAllSE()
{
	for (auto& w : m_activeSe)
	{
		if (auto s = w.lock()) { if (s->IsPlaying() && !s->IsPause()) { s->Pause(); } }
	}
}

void SoundManager::ResumeAllSE()
{
	for (auto& w : m_activeSe)
	{
		if (auto s = w.lock()) { if (s->IsPause()) { s->Resume(); } }
	}
}

//==========================================================
// SE 差し替えテーブル（論理ID → ファイル）
//==========================================================
namespace
{
	// SeId の並び順に対応する（表示名, 既定ファイル）。
	struct SeDef { const char* label; const char* def; };
	const SeDef kSeDefs[] =
	{
		{ "Jump (ジャンプ)",          SoundConst::SeJump       },
		{ "Coin (コイン取得)",        SoundConst::SeCoin       },
		{ "Pickup (エメラルド取得)",  SoundConst::SePickup     },
		{ "Checkpoint (旗)",          SoundConst::SeCheckpoint },
		{ "Damage (被ダメージ)",      SoundConst::SeDamage     },
		{ "Death (死亡)",             SoundConst::SeDeath      },
		{ "Stomp (踏みつけ)",         SoundConst::SeStomp      },
		{ "Core (重力コア取得)",      SoundConst::SeCore       },
		{ "Clear (ステージクリア)",   SoundConst::SeClear      },
		{ "MenuMove (メニュー移動)",  SoundConst::SeMenuMove   },
		{ "MenuDecide (メニュー決定)",SoundConst::SeMenuDecide },
		{ "Land (着地)",              SoundConst::SeLand          },
		{ "Footstep (足音)",          SoundConst::SeFootstep      },
		{ "StageFlyIn (飛んでくる)",  SoundConst::SeStageFlyIn    },
		{ "MenuCancel (戻る/TAB)",    SoundConst::SeMenuCancel    },
		{ "PauseOpen (ポーズ開く)",   SoundConst::SePauseOpen     },
		{ "PauseClose (ポーズ閉じる)",SoundConst::SePauseClose    },
		{ "TitleStart (タイトル開始)",SoundConst::SeTitleStart    },
		{ "StoryAdvance (物語送り)",  SoundConst::SeStoryAdvance  },
		{ "StorySkip (物語スキップ)", SoundConst::SeStorySkip     },
		{ "StageGo (ステージへ)",     SoundConst::SeStageGo       },
		{ "StageDeny (拒否)",         SoundConst::SeStageDeny     },
		{ "ResultAdvance (リザルト送り)", SoundConst::SeResultAdvance },
		{ "Respawn (復活)",           SoundConst::SeRespawn       },
		{ "CoreliaTalk (会話)",       SoundConst::SeCoreliaTalk   },
		{ "UiSend (UIへ送る)",        SoundConst::SeUiSend        },
		{ "UiAbsorb (UI吸収)",        SoundConst::SeUiAbsorb      },
		{ "ParasolGet (パラソル取得)",SoundConst::SeParasolGet    },
	};
	static_assert(sizeof(kSeDefs) / sizeof(kSeDefs[0]) == static_cast<size_t>(SeId::MaxNum),
		"kSeDefs と SeId の数が一致していません");
}

void SoundManager::InitSeTable()
{
	if (m_seInit) { return; }
	m_seInit = true;

	for (size_t i = 0; i < m_seSlots.size(); ++i)
	{
		m_seSlots[i].label = AcpToUtf8(kSeDefs[i].label);   // ラベルもUTF-8化（日本語含む）
		m_seSlots[i].path  = kSeDefs[i].def;                // 既定割り当て（CP932ネイティブ）
	}

	LoadSeAssign();          // 保存があれば上書き
	RefreshSeFileList();     // フォルダ一覧
}

void SoundManager::RefreshSeFileList()
{
	m_seFiles.clear();
	m_seFilesU8.clear();

	namespace fs = std::filesystem;
	std::error_code ec;
	const fs::path dir = u8"Asset/Sound/SE";
	if (!fs::exists(dir, ec)) { return; }

	for (const auto& e : fs::directory_iterator(dir, ec))
	{
		if (!e.is_regular_file()) { continue; }
		const std::string ext = e.path().extension().string();
		std::string lower = ext;
		std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
		if (lower != ".mp3" && lower != ".wav" && lower != ".m4a" && lower != ".aac" && lower != ".wma") { continue; }

		const std::string full = "Asset/Sound/SE/" + e.path().filename().string();   // CP932ネイティブ
		m_seFiles.push_back(full);
		m_seFilesU8.push_back(AcpToUtf8(BaseNameOf(full)));
	}
}

const std::string& SoundManager::SePath(SeId id)
{
	InitSeTable();
	const size_t i = static_cast<size_t>(id);
	return m_seSlots[i].path;
}

void SoundManager::PlaySE(SeId id, float vol)
{
	PlaySE(SePath(id), vol);
}

void SoundManager::PlaySE3D(SeId id, const Math::Vector3& pos, float vol)
{
	PlaySE3D(SePath(id), pos, vol);
}

void SoundManager::PreloadAllSE()
{
	InitSeTable();
	for (const auto& s : m_seSlots) { Preload(s.path); }
}

void SoundManager::PreloadAllBgm()
{
	// 通常版＋こもり版の両方をデコード済みにしておく（再生・ポーズ時のカクつき/遅延防止）
	auto pre = [&](const char* path) { Preload(path); Preload(MuffledOf(path)); };
	pre(SoundConst::BgmTitle);
	pre(SoundConst::BgmStory);
	pre(SoundConst::BgmStageSelect);
	pre(SoundConst::BgmShowcase);
	for (int i = 0; i < SoundConst::BgmStageCount; ++i) { pre(SoundConst::BgmStage[i]); }
}

void SoundManager::PreloadAllWithProgress(const std::function<void(float)>& onProgress)
{
	InitSeTable();

	// 読み込む全パスを集める（BGM通常＋こもり、SE）
	std::vector<std::string> list;
	auto addBgm = [&](const char* p) { list.push_back(p); list.push_back(MuffledOf(p)); };
	addBgm(SoundConst::BgmTitle);
	addBgm(SoundConst::BgmStory);
	addBgm(SoundConst::BgmStageSelect);
	addBgm(SoundConst::BgmShowcase);
	for (int i = 0; i < SoundConst::BgmStageCount; ++i) { addBgm(SoundConst::BgmStage[i]); }
	for (const auto& s : m_seSlots) { list.push_back(s.path); }

	const size_t n = list.size();
	for (size_t i = 0; i < n; ++i)
	{
		Preload(list[i]);
		if (onProgress) { onProgress(static_cast<float>(i + 1) / static_cast<float>(n)); }
	}
}

void SoundManager::SaveSeAssign()
{
	std::ofstream ofs(SoundConst::SeAssignFile, std::ios::binary);
	if (!ofs) { return; }
	// 形式： "<index>=<path>"（pathはCP932バイトをそのまま）
	for (size_t i = 0; i < m_seSlots.size(); ++i)
	{
		ofs << i << '=' << m_seSlots[i].path << '\n';
	}
}

void SoundManager::LoadSeAssign()
{
	std::ifstream ifs(SoundConst::SeAssignFile, std::ios::binary);
	if (!ifs) { return; }
	std::string line;
	while (std::getline(ifs, line))
	{
		if (!line.empty() && line.back() == '\r') { line.pop_back(); }
		const size_t eq = line.find('=');
		if (eq == std::string::npos) { continue; }
		const int idx = std::atoi(line.substr(0, eq).c_str());
		if (idx < 0 || idx >= static_cast<int>(m_seSlots.size())) { continue; }
		m_seSlots[idx].path = line.substr(eq + 1);
	}
}

void SoundManager::PlayBGM(std::string_view path, float vol)
{
	m_bgmVol = vol;

	// 同じ論理曲が既に流れていれば何もしない（こもり状態・ダッキングは維持）
	if (m_bgmBasePath == path && m_bgm && !m_bgm->IsStopped())
	{
		// 目標音量は Update 側で m_bgmVol * m_duck から毎フレーム算出する
		return;
	}

	// 新しい論理曲：こもり解除して通常版を再生
	m_bgmBasePath = std::string(path);
	m_muffled     = false;
	StartTrack(path);
}

void SoundManager::StartTrack(std::string_view path)
{
	// いま鳴っている通常版を「フェードアウト枠」へ退避（曲切替のクロスフェード）
	if (m_bgm)
	{
		m_bgmOld = m_bgm;
		m_oldVol = m_curVol;
		m_bgm.reset();
	}
	// こもり版は曲ごと使い捨て（即停止）
	if (m_bgmMuf) { m_bgmMuf->Stop(); m_bgmMuf.reset(); }

	m_bgmPath.clear();
	m_curVol = 0.0f;
	m_mufVol = 0.0f;

	if (!Exists(path)) { return; }   // 未配置なら無音（oldはフェードアウトで消える）

	// 通常版とこもり版を「同時に」ループ再生開始＝以後ずっと同期する。
	// ファイルが無ければ無音（アサート回避）。こもり版と同様にガードする。
	if (Exists(path))
	{
		m_bgm = KdAudioManager::Instance().Play(path, true);
		if (m_bgm) { m_bgm->SetVolume(0.0f); m_bgmPath = std::string(path); }
	}

	const std::string muf = MuffledOf(path);
	if (Exists(muf))
	{
		m_bgmMuf = KdAudioManager::Instance().Play(muf, true);
		if (m_bgmMuf) { m_bgmMuf->SetVolume(0.0f); }
	}
}

void SoundManager::StopBGM()
{
	if (m_bgm)
	{
		m_bgmOld = m_bgm;
		m_oldVol = std::max(m_curVol, m_mufVol);   // 鳴っていた方の音量からフェードアウト
		m_bgm.reset();
	}
	if (m_bgmMuf) { m_bgmMuf->Stop(); m_bgmMuf.reset(); }
	m_bgmPath.clear();
	m_bgmBasePath.clear();
	m_muffled = false;
	m_curVol  = 0.0f;
	m_mufVol  = 0.0f;
}

void SoundManager::SetBgmMuffle(bool on)
{
	if (on == m_muffled) { return; }       // 変化なし
	// こもり版が無ければ切り替えない（通常版のまま）
	if (on && !m_bgmMuf) { return; }
	// 再生はやり直さず、音量クロスフェードのターゲットだけ変える（位置は同期したまま）
	m_muffled = on;
}

void SoundManager::SetBgmVolumeScale(float scale)
{
	if (scale < 0.0f) { scale = 0.0f; }
	m_duck = scale;   // 実際の音量反映は Update（m_bgmVol * m_duck へフェード）
}

void SoundManager::Update(float dt)
{
	const float fade = SoundConst::BgmFadeSpeed * dt;

	// 全体の目標音量＝基準×ダッキング×ユーザー設定(マスター×BGM)
	const float full = m_bgmVol * m_duck * m_masterVol * m_bgmUserVol;
	// こもり中はこもり版を、通常時は通常版を鳴らす（もう片方は0へ）。両者は同期再生中。
	const float nTarget = m_muffled ? 0.0f : full;
	const float mTarget = m_muffled ? full : 0.0f;

	auto approach = [&](float cur, float target) {
		if (cur < target) { return std::min(cur + fade, target); }
		return std::max(cur - fade, target);
	};

	if (m_bgm)    { m_curVol = approach(m_curVol, nTarget); m_bgm->SetVolume(m_curVol); }
	if (m_bgmMuf) { m_mufVol = approach(m_mufVol, mTarget); m_bgmMuf->SetVolume(m_mufVol); }

	// 退避BGM：0へフェードアウト → 消す
	if (m_bgmOld)
	{
		m_oldVol = std::max(m_oldVol - fade, 0.0f);
		m_bgmOld->SetVolume(m_oldVol);
		if (m_oldVol <= 0.0f)
		{
			m_bgmOld->Stop();
			m_bgmOld.reset();
		}
	}
}

//==========================================================
// ImGui：SE のホットリロード編集パネル（全シーンで表示）
//==========================================================
void SoundManager::DrawSeEditorGui()
{
	InitSeTable();

	// F5 で表示/非表示をトグル（デフォルト非表示）
	const bool f5 = (GetAsyncKeyState(VK_F5) & 0x8000) != 0;
	if (f5 && !m_f5Prev) { m_seEditorVisible = !m_seEditorVisible; }
	m_f5Prev = f5;
	if (!m_seEditorVisible) { return; }

	if (!ImGui::Begin(reinterpret_cast<const char*>(u8"SE Editor (ホットリロード)")))
	{
		ImGui::End();
		return;
	}

	// 上段：フォルダ再読込／保存
	if (ImGui::Button(reinterpret_cast<const char*>(u8"フォルダ再読込"))) { RefreshSeFileList(); }
	ImGui::SameLine();
	if (ImGui::Button(reinterpret_cast<const char*>(u8"割り当てを保存"))) { SaveSeAssign(); }
	ImGui::SameLine();
	ImGui::TextDisabled("(%d files)", static_cast<int>(m_seFiles.size()));
	ImGui::Separator();

	for (size_t i = 0; i < m_seSlots.size(); ++i)
	{
		auto& slot = m_seSlots[i];
		ImGui::PushID(static_cast<int>(i));

		// ラベル（UTF-8）
		ImGui::TextUnformatted(slot.label.c_str());

		// 現在の割り当てに一致する選択肢インデックスを探す
		int cur = -1;
		for (size_t f = 0; f < m_seFiles.size(); ++f)
		{
			if (m_seFiles[f] == slot.path) { cur = static_cast<int>(f); break; }
		}

		// コンボ（差し替え＝ホットリロード）。プレビューは現割り当てのファイル名。
		const std::string preview = AcpToUtf8(BaseNameOf(slot.path));
		ImGui::SetNextItemWidth(280.0f);
		if (ImGui::BeginCombo("##file", preview.c_str()))
		{
			for (size_t f = 0; f < m_seFilesU8.size(); ++f)
			{
				const bool sel = (static_cast<int>(f) == cur);
				if (ImGui::Selectable(m_seFilesU8[f].c_str(), sel))
				{
					slot.path = m_seFiles[f];     // 即差し替え（次の再生から反映）
					Preload(slot.path);           // カクつき防止
				}
				if (sel) { ImGui::SetItemDefaultFocus(); }
			}
			ImGui::EndCombo();
		}

		// 試聴
		ImGui::SameLine();
		if (ImGui::Button(reinterpret_cast<const char*>(u8"▶ 試聴")))
		{
			PlaySE(static_cast<SeId>(i), SoundConst::SeVolume);
		}

		// 存在しないファイルは警告
		if (!Exists(slot.path))
		{
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), reinterpret_cast<const char*>(u8"(無)"));
		}

		ImGui::PopID();
		ImGui::Separator();
	}

	ImGui::End();
}
