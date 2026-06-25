#pragma once
#include <Windows.h>

//==========================================================
// MouseInput
// OSマウス座標を「描画解像度に依存しない」形へ変換するユーティリティ。
//  ・ウィンドウのクライアント実サイズを基準に正規化(0..1)するので、
//    ウィンドウ／ボーダレス全画面／解像度変更のどれでもズレない。
//  ・スワップチェーンはクライアント領域いっぱいに presentされるため、
//    クライアント正規化座標 = 画面内の相対位置になる。
//==========================================================
namespace MouseInput
{
	// OSカーソル → クライアント正規化座標（x:右0→1 / y:下0→1）。
	// 取得できなければ false。
	inline bool ClientNDC(HWND hwnd, float& outNx, float& outNy)
	{
		if (!hwnd) { return false; }
		POINT p;
		if (!GetCursorPos(&p)) { return false; }
		ScreenToClient(hwnd, &p);
		RECT rc;
		GetClientRect(hwnd, &rc);
		const float w = static_cast<float>(rc.right - rc.left);
		const float h = static_cast<float>(rc.bottom - rc.top);
		if (w <= 0.0f || h <= 0.0f) { return false; }
		outNx = static_cast<float>(p.x) / w;
		outNy = static_cast<float>(p.y) / h;
		return true;
	}
}
