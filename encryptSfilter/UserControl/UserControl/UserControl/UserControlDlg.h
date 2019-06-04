// UserControlDlg.h : ͷ�ļ�
//

#pragma once
#include "afxwin.h"


// CUserControlDlg �Ի���
class CUserControlDlg : public CDialog
{
// ����
public:
	CUserControlDlg(CWnd* pParent = NULL);	// ��׼���캯��

// �Ի�������
	enum { IDD = IDD_USERCONTROL_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV ֧��


// ʵ��
protected:
	HICON m_hIcon;

	// ���ɵ���Ϣӳ�亯��
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	CCheckListBox m_rulelist;
	afx_msg void OnLbnSelchangeRuleList();
	afx_msg void OnBnClickedAddProc();
	CString str;
	afx_msg void OnBnClickedOk();
	afx_msg void OnBnClickedCancel();
};

#define IOCTL_SET_PROC_RULE CTL_CODE(\
	FILE_DEVICE_FILE_SYSTEM, \
	0x800, \
	METHOD_BUFFERED, \
	FILE_ANY_ACCESS)

#define IOCTL_SET_DIR_RULE CTL_CODE(\
	FILE_DEVICE_FILE_SYSTEM, \
	0x801, \
	METHOD_BUFFERED, \
	FILE_ANY_ACCESS)

