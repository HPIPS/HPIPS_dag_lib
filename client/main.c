/* xdag main, T13.654-T13.895 $DVS:time$ */
#include "init.h"
#include <new.h>

int main(int argc, char **argv)
{
	//typedef int(*FUNA)(int , char *);
	//HMODULE hMod = LoadLibrary(TEXT("hpips_dag_lib.dll"));//dll路径
	//if (hMod)
	//{
	//	FUNA addfun = (FUNA)GetProcAddress(hMod, TEXT("init"));//直接使用原工程函数名 
	//	//init(argc, argv);
	//	FreeLibrary(hMod);
	//}
	//argv = new char*[200];
	//argc = 6;
	////argv[1] = "-P";
	////argv[2] = "127.0.0.1:2323";
	//argv[1] = "-t";
	//argv[2] = "-P";
	//argv[3] = "127.0.0.1:2323";
	//argv[4] = "-threads";
	//argv[5] = "1";
	dag_init(argc, argv, 0);
	return 0;
}
