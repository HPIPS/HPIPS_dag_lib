/* xdag main, T13.654-T13.895 $DVS:time$ */
#include "init.h"
int main(int argc, char **argv)
{
	//typedef int(*FUNA)(int , char *);
	//HMODULE hMod = LoadLibrary(TEXT("hpips_dag_lib.dll"));//dll·��
	//if (hMod)
	//{
	//	FUNA addfun = (FUNA)GetProcAddress(hMod, TEXT("init"));//ֱ��ʹ��ԭ���̺����� 
	//	//init(argc, argv);
	//	FreeLibrary(hMod);
	//}
	dag_init(argc, argv, 0);
	return 0;
}
