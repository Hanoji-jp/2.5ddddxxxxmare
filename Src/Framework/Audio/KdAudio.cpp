#include "KdAudio.h"

// ── mp3 等を PCM へデコードするための Media Foundation ──
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <wrl/client.h>
#include <vector>
#include <algorithm>
#include <cctype>
#include <cstring>
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

namespace
{
	// 拡張子（小文字）で末尾一致
	bool EndsWithExtCI(std::string_view name, std::string_view ext)
	{
		if (name.size() < ext.size()) { return false; }
		for (size_t i = 0; i < ext.size(); ++i)
		{
			const char a = static_cast<char>(std::tolower(static_cast<unsigned char>(name[name.size() - ext.size() + i])));
			const char b = static_cast<char>(std::tolower(static_cast<unsigned char>(ext[i])));
			if (a != b) { return false; }
		}
		return true;
	}

	// Media Foundation で音声ファイル(mp3/m4a/wma等)を 16bit PCM へ全デコードする。
	// 成功時：outPcm に生PCM、outWfx に PCM フォーマットを返す。
	bool DecodeAudioToPcm(const wchar_t* path, std::vector<uint8_t>& outPcm, WAVEFORMATEX& outWfx)
	{
		using Microsoft::WRL::ComPtr;

		ComPtr<IMFSourceReader> reader;
		if (FAILED(MFCreateSourceReaderFromURL(path, nullptr, reader.GetAddressOf()))) { return false; }

		// 音声ストリームのみ選択
		reader->SetStreamSelection(static_cast<DWORD>(MF_SOURCE_READER_ALL_STREAMS), FALSE);
		reader->SetStreamSelection(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), TRUE);

		// 出力を 16bit PCM に指定（デコーダが自動で挟まる）
		ComPtr<IMFMediaType> pcmType;
		if (FAILED(MFCreateMediaType(pcmType.GetAddressOf()))) { return false; }
		pcmType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
		pcmType->SetGUID(MF_MT_SUBTYPE,    MFAudioFormat_PCM);
		pcmType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
		if (FAILED(reader->SetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), nullptr, pcmType.Get())))
		{
			return false;
		}

		// 実際に決まった出力フォーマットを取得
		ComPtr<IMFMediaType> actual;
		if (FAILED(reader->GetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), actual.GetAddressOf())))
		{
			return false;
		}

		UINT32 channels = 0, sampleRate = 0, bits = 16;
		actual->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS,     &channels);
		actual->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &sampleRate);
		actual->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE,  &bits);
		if (channels == 0 || sampleRate == 0 || bits == 0) { return false; }

		ZeroMemory(&outWfx, sizeof(outWfx));
		outWfx.wFormatTag      = WAVE_FORMAT_PCM;
		outWfx.nChannels       = static_cast<WORD>(channels);
		outWfx.nSamplesPerSec  = sampleRate;
		outWfx.wBitsPerSample  = static_cast<WORD>(bits);
		outWfx.nBlockAlign     = static_cast<WORD>(channels * bits / 8);
		outWfx.nAvgBytesPerSec = sampleRate * outWfx.nBlockAlign;
		outWfx.cbSize          = 0;

		// 全サンプルを読み出して連結
		for (;;)
		{
			DWORD flags = 0;
			ComPtr<IMFSample> sample;
			if (FAILED(reader->ReadSample(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM),
				0, nullptr, &flags, nullptr, sample.GetAddressOf())))
			{
				return false;
			}
			if (flags & MF_SOURCE_READERF_ENDOFSTREAM) { break; }
			if (!sample) { continue; }

			ComPtr<IMFMediaBuffer> buffer;
			if (FAILED(sample->ConvertToContiguousBuffer(buffer.GetAddressOf()))) { return false; }

			BYTE* data = nullptr; DWORD len = 0;
			if (FAILED(buffer->Lock(&data, nullptr, &len))) { return false; }
			outPcm.insert(outPcm.end(), data, data + len);
			buffer->Unlock();
		}

		return !outPcm.empty();
	}
}

// ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### #####
//
// KdAudioManager
// 
// ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### #####

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 初期化
// ・DirectXAudioEngineの初期化 
// ・3Dリスナーの設定
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdAudioManager::Init()
{
	// AudioEngine初期化
	DirectX::AUDIO_ENGINE_FLAGS eflags = DirectX::AudioEngine_ReverbUseFilters;

	m_audioEng = std::make_unique<DirectX::AudioEngine>(eflags);
	m_audioEng->SetReverb(DirectX::Reverb_Default);

	m_listener.OrientFront = { 0, 0, 1 };

	// Media Foundation 初期化（mp3 等のデコード用）。COM は AudioEngine 生成時点で初期化済み前提。
	if (!m_mfInitialized)
	{
		if (SUCCEEDED(MFStartup(MF_VERSION, MFSTARTUP_LITE)))
		{
			m_mfInitialized = true;
		}
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 更新
// ・DirectXAudioEngineの更新
// ・プレイリストから不要なサウンドインスタンスを削除
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdAudioManager::Update()
{
	// 実はこれを実行しなくても音はなる:定期的に実行する必要はある
	if (m_audioEng != nullptr)
	{
		m_audioEng->Update();
	}

	// ストップさせたインスタンスは終了したと判断してリストから削除
	for (auto iter = m_playList.begin(); iter != m_playList.end();)
	{
		if (iter->second->IsStopped())
		{
			iter = m_playList.erase(iter);

			continue;
		}

		++iter;
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// リスナーの座標と正面方向の設定
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdAudioManager::SetListnerMatrix(const Math::Matrix& mWorld)
{
	// 座標
	m_listener.SetPosition(mWorld.Translation());

	// 正面方向
	m_listener.OrientFront = mWorld.Backward();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 2Dサウンドの再生
// ・サウンドアセットの取得orロード
// ・再生用インスタンスの生成
// ・管理用プレイリストへの追加
// ・戻り値で再生インスタンスを取得可能（音量・ピッチなどを変更する場合に必要
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
std::shared_ptr<KdSoundInstance> KdAudioManager::Play(std::string_view rName, bool loop)
{
	if (!m_audioEng) { return nullptr; }

	std::shared_ptr<KdSoundEffect> soundData = GetSound(rName);

	if (!soundData) { return nullptr; }

	std::shared_ptr<KdSoundInstance> instance = std::make_shared<KdSoundInstance>(soundData);

	if(!instance->CreateInstance()){ return nullptr; }

	instance->Play(loop);

	AddPlayList(instance);

	return instance;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 3Dサウンドの再生
// ・サウンドアセットの取得orロード
// ・再生用インスタンスの生成、3D座標のセット
// ・管理用プレイリストへの追加
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
std::shared_ptr<KdSoundInstance3D> KdAudioManager::Play3D(std::string_view rName, const Math::Vector3& rPos, bool loop)
{
	if (!m_audioEng) { return nullptr; }

	std::shared_ptr<KdSoundEffect> soundData = GetSound(rName);

	if (!soundData) { return nullptr; }

	std::shared_ptr<KdSoundInstance3D> instance = std::make_shared<KdSoundInstance3D>(soundData, m_listener);

	if (!instance->CreateInstance()) { return nullptr; }

	instance->Play(loop);

	instance->SetPos(rPos);

	AddPlayList(instance);

	return instance;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 再生リストの全ての音を停止する
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdAudioManager::StopAllSound()
{
	auto it = m_playList.begin();
	while (it != m_playList.end()) {
		(*it).second->Stop();
		++it;
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 再生リストの全ての音を一時停止する
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdAudioManager::PauseAllSound()
{
	auto it = m_playList.begin();
	while (it != m_playList.end()) {
		(*it).second->Pause();
		++it;
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 再生リストの全ての音を再開する
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdAudioManager::ResumeAllSound()
{
	auto it = m_playList.begin();
	while (it != m_playList.end()) {
		(*it).second->Resume();
		++it;
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 再生中のサウンドの停止・サウンドアセットの解放
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdAudioManager::SoundReset()
{
	StopAllSound();

	m_soundMap.clear();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// サウンドアセットの読み込み
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdAudioManager::LoadSoundAssets(std::initializer_list<std::string_view>& fileNames)
{
	for (std::string_view fileName : fileNames)
	{
		auto itFound = m_soundMap.find(fileName.data());

		// ロード済みならスキップ
		if (itFound != m_soundMap.end()) { continue; }

		// 生成 & 読み込み
		auto newSound = std::make_shared<KdSoundEffect>();
		if (!newSound->Load(fileName, m_audioEng))
		{
			// 読み込み失敗時
			assert(0 && "LoadSoundAssets:ファイル名のデータが存在しません。名前を確認してください。");

			continue;
		}

		// リスト(map)に登録
		m_soundMap.emplace(fileName, newSound);

	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 解放
// ・再生中のプレイリストの解放
// ・サウンドアセットの解放
// ・エンジンの解放
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdAudioManager::Release()
{
	StopAllSound();

	m_playList.clear();

	m_soundMap.clear();

	m_audioEng = nullptr;

	// Media Foundation 終了
	if (m_mfInitialized)
	{
		MFShutdown();
		m_mfInitialized = false;
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// サウンドアセットの取得
// ・
// ・サウンドアセットの解放
// ・エンジンの解放
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
std::shared_ptr<KdSoundEffect> KdAudioManager::GetSound(std::string_view fileName)
{
	// filenameのサウンドアセットがロード済みか？
	auto itFound = m_soundMap.find(fileName.data());

	// ロード済み
	if (itFound != m_soundMap.end())
	{
		return (*itFound).second;
	}
	else
	{
		// 生成 & 読み込み
		auto newSound = std::make_shared<KdSoundEffect>();
		if (!newSound->Load(fileName, m_audioEng))
		{
			// 読み込み失敗時は、nullを返す
			return nullptr;
		}
		// リスト(map)に登録
		m_soundMap.emplace(fileName, newSound);

		// リソースを返す
		return newSound;
	}
}


// ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### #####
// 
// KdSoundEffect
// 
// ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### #####

// 音データの読み込み
bool KdSoundEffect::Load(std::string_view fileName, const std::unique_ptr<DirectX::AudioEngine>& engine)
{
	if (engine.get() != nullptr)
	{
		try
		{
			// wstringに変換
			std::wstring wFilename = sjis_to_wide(fileName.data());

			// mp3/m4a/wma 等の圧縮音源は Media Foundation で PCM デコードして読み込む。
			// wav は従来どおり DirectXTK で直接読み込む（速い）。
			const bool compressed =
				EndsWithExtCI(fileName, ".mp3") ||
				EndsWithExtCI(fileName, ".m4a") ||
				EndsWithExtCI(fileName, ".aac") ||
				EndsWithExtCI(fileName, ".wma");

			if (compressed)
			{
				std::vector<uint8_t> pcm;
				WAVEFORMATEX wfx{};
				if (!DecodeAudioToPcm(wFilename.c_str(), pcm, wfx))
				{
					assert(0 && "Sound File Decode Error (Media Foundation)");
					return false;
				}

				// [WAVEFORMATEX][PCMデータ] の連続バッファを作る。
				// SoundEffect は wfx ポインタを保持するため、所有権を渡すバッファ内に同居させる。
				const size_t hdr = sizeof(WAVEFORMATEX);
				auto wavData = std::make_unique<uint8_t[]>(hdr + pcm.size());
				std::memcpy(wavData.get(), &wfx, hdr);
				std::memcpy(wavData.get() + hdr, pcm.data(), pcm.size());

				const WAVEFORMATEX* pwfx = reinterpret_cast<const WAVEFORMATEX*>(wavData.get());
				const uint8_t* startAudio = wavData.get() + hdr;

				m_soundEffect = std::make_unique<DirectX::SoundEffect>(
					engine.get(), wavData, pwfx, startAudio, pcm.size());
			}
			else
			{
				// wav 読み込み
				m_soundEffect = std::make_unique<DirectX::SoundEffect>(engine.get(), wFilename.c_str());
			}
		}
		catch (...)
		{
			assert(0 && "Sound File Load Error");

			return false;
		}
	}

	return true;
}


// ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### #####
// 
// KdSoundInstance
// 
// ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### #####

KdSoundInstance::KdSoundInstance(const std::shared_ptr<KdSoundEffect>& soundEffect)
	:m_soundData(soundEffect){}

bool KdSoundInstance::CreateInstance()
{
	if (!m_soundData) { return false; }

	DirectX::SOUND_EFFECT_INSTANCE_FLAGS flags = DirectX::SoundEffectInstance_Default;

	m_instance = (m_soundData->CreateInstance(flags));

	return true;
}

void KdSoundInstance::Play(bool loop)
{
	if (!m_instance) { return; }

	// 再生状態がどんな状況か分からないので一度停止してから
	Stop();

	m_instance->Play(loop);
}

void KdSoundInstance::SetVolume(float vol)
{
	if (m_instance == nullptr) { return; }

	m_instance->SetVolume(vol);
}

void KdSoundInstance::SetPitch(float pitch)
{
	if (m_instance == nullptr) { return; }

	m_instance->SetPitch(pitch);
}

bool KdSoundInstance::IsPlaying()
{
	if (!m_instance) { return false; }

	return (m_instance->GetState() == DirectX::SoundState::PLAYING);
}

bool KdSoundInstance::IsPause()
{
	if (!m_instance) { return false; }

	return (m_instance->GetState() == DirectX::SoundState::PAUSED);
}

bool KdSoundInstance::IsStopped()
{
	if (!m_instance) { return false; }

	return (m_instance->GetState() == DirectX::SoundState::STOPPED);
}



// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 
// KdSoundInstance3D
// 
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////

KdSoundInstance3D::KdSoundInstance3D(const std::shared_ptr<KdSoundEffect>& soundEffect, const DirectX::AudioListener& ownerListener)
	:KdSoundInstance(soundEffect), m_ownerListener(ownerListener){}

bool KdSoundInstance3D::CreateInstance()
{
	if (!m_soundData) { return false; }

	DirectX::SOUND_EFFECT_INSTANCE_FLAGS flags = DirectX::SoundEffectInstance_Default |
		DirectX::SoundEffectInstance_Use3D | DirectX::SoundEffectInstance_ReverbUseFilters;

	m_instance = (m_soundData->CreateInstance(flags));

	return true;
}

void KdSoundInstance3D::Play(bool loop)
{
	if (!m_instance) { return; }

	KdSoundInstance::Play(loop);
}

void KdSoundInstance3D::SetPos(const Math::Vector3& rPos)
{
	if (!m_instance) { return; }

	m_emitter.SetPosition(rPos);

	m_instance->Apply3D(m_ownerListener, m_emitter, false);
}

void KdSoundInstance3D::SetCurveDistanceScaler(float val)
{
	if (!m_instance) { return; }

	m_emitter.CurveDistanceScaler = val;

	m_instance->Apply3D(m_ownerListener, m_emitter, false);
}
