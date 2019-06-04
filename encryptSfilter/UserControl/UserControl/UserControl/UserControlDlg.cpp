// UserControlDlg.cpp : ʵ���ļ�
//

#include "stdafx.h"
#include "UserControl.h"
#include "UserControlDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// ����Ӧ�ó��򡰹��ڡ��˵���� CAboutDlg �Ի���

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// �Ի�������
	enum { IDD = IDD_ABOUTBOX };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV ֧��

// ʵ��
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
END_MESSAGE_MAP()


// CUserControlDlg �Ի���




CUserControlDlg::CUserControlDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CUserControlDlg::IDD, pParent)
	, str(_T(""))
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CUserControlDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_RULE_LIST, m_rulelist);
	DDX_Text(pDX, IDC_EDIT_PROC, str);
}

BEGIN_MESSAGE_MAP(CUserControlDlg, CDialog)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	//}}AFX_MSG_MAP
	ON_LBN_SELCHANGE(IDC_RULE_LIST, &CUserControlDlg::OnLbnSelchangeRuleList)
	ON_BN_CLICKED(IDC_ADD_PROC, &CUserControlDlg::OnBnClickedAddProc)
	ON_BN_CLICKED(IDOK, &CUserControlDlg::OnBnClickedOk)
	ON_BN_CLICKED(IDCANCEL, &CUserControlDlg::OnBnClickedCancel)
END_MESSAGE_MAP()


// CUserControlDlg ��Ϣ�������

BOOL CUserControlDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// ��������...���˵�����ӵ�ϵͳ�˵��С�

	// IDM_ABOUTBOX ������ϵͳ���Χ�ڡ�
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		CString strAboutMenu;
		strAboutMenu.LoadString(IDS_ABOUTBOX);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// ���ô˶Ի����ͼ�ꡣ��Ӧ�ó��������ڲ��ǶԻ���ʱ����ܽ��Զ�
	//  ִ�д˲���
	SetIcon(m_hIcon, TRUE);			// ���ô�ͼ��
	SetIcon(m_hIcon, FALSE);		// ����Сͼ��

	// TODO: �ڴ���Ӷ���ĳ�ʼ������
	m_rulelist.SetCheckStyle(BS_CHECKBOX);
	m_rulelist.AddString(_T("explorer.exe"));
	m_rulelist.AddString(_T("notepad.exe"));
	m_rulelist.AddString(_T("wordpad.exe"));
	m_rulelist.AddString(_T("WINWORD.EXE"));
	

	return TRUE;  // ���ǽ��������õ��ؼ������򷵻� TRUE
}

void CUserControlDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialog::OnSysCommand(nID, lParam);
	}
}

// �����Ի��������С����ť������Ҫ����Ĵ���
//  �����Ƹ�ͼ�ꡣ����ʹ���ĵ�/��ͼģ�͵� MFC Ӧ�ó���
//  �⽫�ɿ���Զ���ɡ�

void CUserControlDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // ���ڻ��Ƶ��豸������

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// ʹͼ���ڹ����������о���
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// ����ͼ��
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

//���û��϶���С������ʱϵͳ���ô˺���ȡ�ù��
//��ʾ��
HCURSOR CUserControlDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}


void CUserControlDlg::OnLbnSelchangeRuleList()
{
	int i=m_rulelist.GetCurSel();
	if(i<0)return;
	if(m_rulelist.GetCheck(i)<1)
		m_rulelist.SetCheck(i,1);
	else
		m_rulelist.SetCheck(i,0);
}

void CUserControlDlg::OnBnClickedAddProc()
{
	CEdit*   pEdit;   
	pEdit=(CEdit*) GetDlgItem(IDC_EDIT_PROC);   
	pEdit->GetWindowText(str);
	m_rulelist.AddString(str);
}

void CUserControlDlg::OnBnClickedOk()
{
	CString str_proc,str_proclist;

	int proc_num = m_rulelist.GetCount();
	
	for (int i=0;i<proc_num;i++)
	{
		if(m_rulelist.GetCheck(i))
		{
			m_rulelist.GetText(i,str_proc);
			str_proclist = str_proclist + str_proc + _T(";");
		}
	}
	int nLen = str_proclist.GetLength();
	str_proclist = str_proclist.Left(nLen-1);
	PWCHAR InputBuffer = str_proclist.GetBuffer();
	//printf("%wZ\n",InputBuffer);
	//WCHAR pInputBuffer[1024] = str_proclist.AllocSysString();
	//WCHAR InputBuffer[1024] = _T("1234567890");

	//�����Ǵ�Sfilter��CDO����ͨ��
	
	HANDLE hDevice = 
		CreateFile(_T("\\\\.\\EncryptSystem"),
		GENERIC_READ | GENERIC_WRITE,
		0,		// share mode none
		NULL,	// no security
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL );		// no template

	if (hDevice == INVALID_HANDLE_VALUE)
	{
		AfxMessageBox(_T("Open failed!"));
		//CloseHandle(hDevice);
		OnOK();
		return;
	}

	UCHAR OutputBuffer[10];
	DWORD dwOutput;
	if(! DeviceIoControl(hDevice, IOCTL_SET_PROC_RULE, InputBuffer, 1024, &OutputBuffer, 10, &dwOutput, NULL))
	{	
		
		CString errorcode;
		errorcode.Format(_T("%d"),GetLastError());
		//AfxMessageBox(_T("ͨ��ʧ��!"));
		AfxMessageBox(errorcode);
		CloseHandle(hDevice);
		hDevice = INVALID_HANDLE_VALUE;
		OnOK();
		return;
		
		//CloseHandle(hDevice);		

	}
	AfxMessageBox(_T("�������óɹ�!"));
	CloseHandle(hDevice);
	

	OnOK();
}

void CUserControlDlg::OnBnClickedCancel()
{
	// TODO: �ڴ���ӿؼ�֪ͨ����������
	OnCancel();
}
