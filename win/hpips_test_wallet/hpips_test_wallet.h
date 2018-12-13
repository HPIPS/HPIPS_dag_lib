
// hpips_test_wallet.h: hpips_test_wallet 应用程序的主头文件
//
#ifndef   HPIPS_TEST_WALLET        
#define   HPIPS_TEST_WALLET   
#ifndef __AFXWIN_H__
	#error "在包含此文件之前包含“stdafx.h”以生成 PCH 文件"
#endif

#include "resource.h"       // 主符号


// hpips_test_wallet_App:
// 有关此类的实现，请参阅 hpips_test_wallet.cpp
//

class hpips_test_wallet_App : public CWinApp
{
public:
	hpips_test_wallet_App();


// 重写
public:
	virtual BOOL InitInstance();

	// 实施
	DECLARE_MESSAGE_MAP()
};

extern hpips_test_wallet_App theApp;

#endif  