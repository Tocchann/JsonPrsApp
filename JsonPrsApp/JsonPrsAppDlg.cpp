
// JsonPrsAppDlg.cpp : 実装ファイル
//

#include "stdafx.h"
#include "JsonPrsApp.h"
#include "JsonPrsAppDlg.h"
#include "afxdialogex.h"

#include "ParseJSON.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// アプリケーションのバージョン情報に使われる CAboutDlg ダイアログ

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// ダイアログ データ
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV サポート

// 実装
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CJsonPrsAppDlg ダイアログ



CJsonPrsAppDlg::CJsonPrsAppDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_JSONPRSAPP_DIALOG, pParent)
	, m_jsonFile( _T( "" ) )
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CJsonPrsAppDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange( pDX );
	DDX_Text( pDX, IDC_EDIT_JSONFILE, m_jsonFile );
	DDX_Control( pDX, IDC_LIST_JSONDATA, m_listJsonData );
	DDX_Control( pDX, IDC_CHECK_TEXT_ESCAPE, m_chkTextEscape );
}

BEGIN_MESSAGE_MAP(CJsonPrsAppDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_BN_CLICKED( IDC_BUTTON_SEL_JSONFILE, &CJsonPrsAppDlg::OnBnClickedButtonSelJsonfile )
	ON_BN_CLICKED( IDC_CHECK_TEXT_ESCAPE, &CJsonPrsAppDlg::OnBnClickedCheckTextEscape )
END_MESSAGE_MAP()


// CJsonPrsAppDlg メッセージ ハンドラー

BOOL CJsonPrsAppDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// "バージョン情報..." メニューをシステム メニューに追加します。

	// IDM_ABOUTBOX は、システム コマンドの範囲内になければなりません。
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != nullptr)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// このダイアログのアイコンを設定します。アプリケーションのメイン ウィンドウがダイアログでない場合、
	//  Framework は、この設定を自動的に行います。
	SetIcon(m_hIcon, TRUE);			// 大きいアイコンの設定
	SetIcon(m_hIcon, FALSE);		// 小さいアイコンの設定

	m_chkTextEscape.SetCheck( BST_CHECKED );

	// TODO: 初期化をここに追加します。
	return TRUE;  // フォーカスをコントロールに設定した場合を除き、TRUE を返します。
}

void CJsonPrsAppDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}
#include <atlpath.h>
#include <atlfile.h>

static LPCTSTR NotificationIdToString( Wankuma::JSON::NotificationId id )
{
	switch( id )
	{
	case Wankuma::JSON::NotificationId::StartParse:		return _T( "StartParse " );
	case Wankuma::JSON::NotificationId::EndParse:		return _T( "EndParse   " );
	case Wankuma::JSON::NotificationId::StartObject:	return _T( "StartObject" );
	case Wankuma::JSON::NotificationId::EndObject:		return _T( "EndObject  " );
	case Wankuma::JSON::NotificationId::StartArray:		return _T( "StartArray " );
	case Wankuma::JSON::NotificationId::EndArray:		return _T( "EndArray   " );
	case Wankuma::JSON::NotificationId::Comma:			return _T( "Comma      " );
	case Wankuma::JSON::NotificationId::Key:			return _T( "Key        " );
	case Wankuma::JSON::NotificationId::String:			return _T( "String     " );
	case Wankuma::JSON::NotificationId::Digit:			return _T( "Digit      " );
	case Wankuma::JSON::NotificationId::True:			return _T( "True       " );
	case Wankuma::JSON::NotificationId::False:			return _T( "False      " );
	case Wankuma::JSON::NotificationId::Null:			return _T( "Null       " );
	}
	_assume( 0 );	//	あり得ないのでマーキング
}
void CJsonPrsAppDlg::OnOK()
{
	if( !UpdateData() )
	{
		return;
	}
	if( m_jsonFile.IsEmpty() )
	{
		AfxMessageBox( _T( "ファイルが指定されていません。" ) );
		return;
	}
	if( !ATLPath::FileExists( m_jsonFile ) )
	{
		AfxMessageBox( m_jsonFile + _T( "\n\nファイルが存在しません。" ) );
		return;
	}
	CWaitCursor	wait;
	m_listJsonData.ResetContent();
	//	JSONデータは、BOMなしのUTF8という決まりなので(逸脱は許しません！)、無条件にそういうファイルだと想定してオープンする
	bool textEscape = (m_chkTextEscape.GetCheck() == BST_CHECKED);
	if( textEscape ){
		std::wstring value = L"abc:\r\n\tあ\\いう/えお";
		auto escapeStr1 = Wankuma::JSON::EscapeString( value, true );
		m_listJsonData.AddString( (value + L" -> " + escapeStr1).c_str() );
		auto escapeStr2 = Wankuma::JSON::EscapeString( value, false );
		m_listJsonData.AddString( (value + L" -> " + escapeStr2).c_str() );
	}
	ATL::CAtlFile file;
	auto hRes = file.Create( m_jsonFile, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING );
	if( FAILED( hRes ) )
	{
		_com_error	exp( hRes );
		AfxMessageBox( exp.ErrorMessage() );
		return;
	}
	ATL::CAtlFileMapping<char>	fileMap;
	hRes = fileMap.MapFile( file );
	if( FAILED( hRes ) )
	{
		_com_error	exp( hRes );
		AfxMessageBox( exp.ErrorMessage() );
		return;
	}
	std::string_view jsonData( fileMap, fileMap.GetMappingSize() );
	int nestLevel = 0;

	Wankuma::JSON::ParseJSON( jsonData, [&]( Wankuma::JSON::NotificationId id, const std::string_view& value )
	{
		CString lineData = NotificationIdToString( id );
		lineData += _T( '|' );
		for( int level = 0 ; level < nestLevel ; ++level )
		{
			lineData += _T(' ');
		}
		if( textEscape )
		{
			
			std::string valueStr( Wankuma::JSON::UnescapeString( value ) );
			lineData += Wankuma::JSON::UnescapeWstring( value ).c_str();	//	直接UNICODEに変換
		}
		//	エスケープ処理を行わない場合は、そのままセットする
		else
		{
			std::string valueStr( value );
			lineData += CA2CT( valueStr.c_str(), CP_UTF8 );
		}
		if( id == Wankuma::JSON::NotificationId::Key )
		{
			lineData += _T( " :" );	//	これだけ通知が来ない
		}
		TRACE( _T( "%s\n" ), NotificationIdToString( id ), (LPCTSTR)lineData );
		int newItem = m_listJsonData.AddString( lineData );
		m_listJsonData.SetSel( newItem );
		if( newItem % 10 == 0 )
		{
			m_listJsonData.UpdateWindow();	//	アイテムを選択することでザクザクと進める
		}
		switch( id )
		{
		case Wankuma::JSON::NotificationId::StartArray:
		case Wankuma::JSON::NotificationId::StartObject:
			nestLevel++;
			break;
		case Wankuma::JSON::NotificationId::EndArray:
		case Wankuma::JSON::NotificationId::EndObject:
			nestLevel--;
			_ASSERTE( nestLevel >= 0 );
			break;
		}
		return true;
	} );
}


void CJsonPrsAppDlg::OnBnClickedButtonSelJsonfile()
{
	UpdateData();
	CFileDialog	dlg( TRUE, _T( ".json" ), m_jsonFile, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, _T( "json ファイル|*.json|すべてのファイル|*.*||" ), this );
	if( dlg.DoModal() == IDOK )
	{
		m_jsonFile = dlg.GetPathName();
		UpdateData( FALSE );
	}
}
void CJsonPrsAppDlg::OnBnClickedCheckTextEscape()
{
	if( m_listJsonData.GetCount() != 0 )
	{
		OnOK();	//	すでにセットされている場合は設定を反映するためリストを作り直す
	}
}
