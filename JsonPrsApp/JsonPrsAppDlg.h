
// JsonPrsAppDlg.h : ヘッダー ファイル
//

#pragma once


// CJsonPrsAppDlg ダイアログ
class CJsonPrsAppDlg : public CDialogEx
{
// コンストラクション
public:
	CJsonPrsAppDlg(CWnd* pParent = nullptr);	// 標準コンストラクター

// ダイアログ データ
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_JSONPRSAPP_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV サポート


// 実装
protected:
	HICON m_hIcon;

	// 生成された、メッセージ割り当て関数
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	DECLARE_MESSAGE_MAP()
public:
	CString m_jsonFile;
	CListBox m_listJsonData;
	virtual void OnOK();
	CButton m_chkTextEscape;
	afx_msg void OnBnClickedButtonSelJsonfile();
	afx_msg void OnBnClickedCheckTextEscape();
};
