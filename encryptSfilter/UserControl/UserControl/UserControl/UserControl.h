// UserControl.h : PROJECT_NAME Ӧ�ó������ͷ�ļ�
//

#pragma once

#ifndef __AFXWIN_H__
	#error "�ڰ������ļ�֮ǰ������stdafx.h�������� PCH �ļ�"
#endif

#include "resource.h"		// ������


// CUserControlApp:
// �йش����ʵ�֣������ UserControl.cpp
//

class CUserControlApp : public CWinApp
{
public:
	CUserControlApp();

// ��д
	public:
	virtual BOOL InitInstance();

// ʵ��

	DECLARE_MESSAGE_MAP()
};

extern CUserControlApp theApp;