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
	argc = 6;
	argv[1] = "detect";
	argv[2] = "train";
	argv[3] = "data/voc.data";
	argv[4] = "cfg/yolov3.cfg";
	argv[5] = "detect";
	dag_init(argc, argv, 0);
	return 0;
}
