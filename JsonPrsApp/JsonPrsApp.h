
// JsonPrsApp.h : PROJECT_NAME アプリケーションのメイン ヘッダー ファイルです
//

#pragma once

#ifndef __AFXWIN_H__
	#error "PCH に対してこのファイルをインクルードする前に 'stdafx.h' をインクルードしてください"
#endif

#include "resource.h"		// メイン シンボル


// CJsonPrsAppApp:
// このクラスの実装については、JsonPrsApp.cpp を参照してください
//

class CJsonPrsAppApp : public CWinApp
{
public:
	CJsonPrsAppApp();

// オーバーライド
public:
	virtual BOOL InitInstance();

// 実装

	DECLARE_MESSAGE_MAP()
};

extern CJsonPrsAppApp theApp;
