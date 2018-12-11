/* time functions, T14.524-T14.582 $DVS:time$ */

#include <sys/time.h>
#include "time.h"
#include "system.h"
#include "utils/utils.h"

xtime_t g_time_limit = DEF_TIME_LIMIT, g_xdag_era = DAG_MAIN_ERA;
extern int g_dag_testnet;

// ����һ��ʱ��������������������Ϊ64�볤��
xtime_t dag_main_time(void)
{
	return MAIN_TIME(dag_get_xtimestamp());
}

// �����������������Ӧ��ʱ����������
xtime_t dag_start_main_time(void)
{
	return MAIN_TIME(g_Ddag_era); //
}

///*Dag ʱ�������ʼ����g_dag_era��ֵ��ֵ
int dag_time_init(void)
{
	if (g_dag_testnet) {
		g_Ddag_era = DAG_TEST_ERA;
	}
	
	return 1;
}

// convert xtime_t to string representation
// minimal length of string buffer `buf` should be 60
void dag_xtime_to_string(xtime_t time, char *buf)
{
	struct tm tm;
	char tmp[64] = {0};
	xtime_t t = time >> 10;
	localtime_r(&t, &tm); //��ʱ��ת������ʱ��
	strftime(tmp, 60, "%Y-%m-%d %H:%M:%S", &tm); //��ʽ����ַ
	sprintf(buf, "%s.%03d", tmp, (int)((time & 0x3ff) * 1000) >> 10); //����ļ���ָ��
}

// convert time to string representation
// minimal length of string buffer `buf` should be 50
void dag_time_to_string(xtime_t time, char* buf)
{
	struct tm tm;
	localtime_r(&time, &tm);
	strftime(buf, 50, "%Y-%m-%d %H:%M:%S", &tm);
}

//����ʱ����������
xtime_t dag_get_xtimestamp(void)
{
	struct timeval tp;

	//��ȡ����ʱ�亯��
	gettimeofday(&tp, 0);

	return (uint64_t)(unsigned long)tp.tv_sec << 10 | ((tp.tv_usec << 10) / 1000000);
}

//����ʱ�亯�������뼶��
uint64_t dag_get_time_ms(void)
{
	struct timeval tp;

	gettimeofday(&tp, 0);

	return (uint64_t)(unsigned long)tp.tv_sec * 1000 + tp.tv_usec / 1000;
}
