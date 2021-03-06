
// hpips_test_wallet.cpp: 定义应用程序的类行为。
//
#pragma once
#include "stdafx.h"
#include "hpips_test_wallet.h"
#include "HPIPS_Dag_WalletDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// hpips_test_wallet_App

BEGIN_MESSAGE_MAP(hpips_test_wallet_App, CWinApp)
	ON_COMMAND(ID_HELP, &CWinApp::OnHelp)
END_MESSAGE_MAP()

// hpips_test_wallet_App 构造
hpips_test_wallet_App::hpips_test_wallet_App() 
{
	//支持重新启动管理器
	m_dwRestartManagerSupportFlags = AFX_RESTART_MANAGER_SUPPORT_RESTART;


	// TODO: 在此处添加构造代码，
	// 将所有重要的初始化放置在 InitInstance 中
}

// 唯一的 hpips_test_wallet_App 对象

hpips_test_wallet_App theApp;


// hpips_test_wallet_App 初始化

BOOL hpips_test_wallet_App::InitInstance()
{
	// InitCommonControlsEx() 如果应用程序在WindowsXP上是必需的
	// 清单指定使用CCMTL32.DLL版本6或更高版本来启用
	// visual styles.  Otherwise, any window creation will fail.
	INITCOMMONCONTROLSEX InitCtrls;
	InitCtrls.dwSize = sizeof(InitCtrls);
	// 将此设置为包含所有要使用的公共控件类
	// 在您的应用程序中。
	InitCtrls.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&InitCtrls);

	CWinApp::InitInstance();

	if (!AfxSocketInit())
	{
		AfxMessageBox(IDP_SOCKETS_INIT_FAILED);
		return FALSE;
	}


	AfxEnableControlContainer();

	// 创建外壳管理器，如果对话框包含
	// 任何shell树视图或shell列表视图控件。
	CShellManager *pShellManager = new CShellManager;

	// 激活“Windows本机”可视化管理器以启用MFC控件中的主题
	CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerWindows));

	// Standard initialization
	// If you are not using these features and wish to reduce the size
	// of your final executable, you should remove from the following
	// the specific initialization routines you do not need
	// Change the registry key under which our settings are stored
	// TODO: You should modify this string to be something appropriate
	// such as the name of your company or organization
	/*
	标准初始化
	如果您不使用这些特性，并希望减小尺寸
	在最后的可执行文件中，应该从下面移除
	您不需要的特定初始化例程
	更改存储设置的注册表项
	TODO：你应该把这个字符串修改成合适的字符串。
	如你的公司或组织的名称
	*/
	SetRegistryKey(_T("HPIPS_DAG"));

	HPIPS_Dag_WalletDlg dlg;
	m_pMainWnd = &dlg;
	g_dlg = &dlg;
	INT_PTR nResponse = dlg.DoModal();
	if (nResponse == IDOK)
	{
		// TODO:此处放置代码以处理对话取消时的OK
	}
	else if (nResponse == IDCANCEL)
	{
		// TODO: 此处放置代码以取消消失对话框时的处理
	}
	else if (nResponse == -1)
	{
		TRACE(traceAppMsg, 0, "Warning: dialog creation failed, so application is terminating unexpectedly.\n");
		TRACE(traceAppMsg, 0, "Warning: if you are using MFC controls on the dialog, you cannot #define _AFX_NO_MFC_CONTROLS_IN_DIALOGS.\n");
	}

	// 删除上面创建的shell管理器。
	if (pShellManager != NULL)
	{
		delete pShellManager;
	}

#if !defined(_AFXDLL) && !defined(_AFX_NO_MFC_CONTROLS_IN_DIALOGS)
	ControlBarCleanUp();
#endif

	// 由于对话框已经关闭，所以返回FALSE，以便退出应用程序，而不是启动应用程序的消息泵。
	return FALSE;
}



