/* time functions, T14.524-T14.582 $DVS:time$ */

#include <sys/time.h>
#include "time.h"
#include "system.h"
#include "utils/utils.h"

time_t g_time_limit = DEF_TIME_LIMIT, g_xdag_era = DAG_MAIN_ERA;
extern int g_dag_testnet;

// returns a time period index, where a period is 64 seconds long
time_t dag_main_time(void)
{
	return MAIN_TIME(dag_get_xtimestamp());
}

// returns the time period index corresponding to the start of the network
time_t dag_start_main_time(void)
{
	return MAIN_TIME(DAG_ERA);
}

///*Dag 时间参数初始化
int dag_time_init(void)
{
	if (g_dag_testnet) {
		g_dag_era = DAG_TEST_ERA;
	}
	
	return 1;
}

// convert xtime_t to string representation
// minimal length of string buffer `buf` should be 60
void dag_xtime_to_string(time_t time, char *buf)
{
	struct tm tm;
	char tmp[64] = {0};
	time_t t = time >> 10;
	localtime_r(&t, &tm);
	strftime(tmp, 60, "%Y-%m-%d %H:%M:%S", &tm);
	sprintf(buf, "%s.%03d", tmp, (int)((time & 0x3ff) * 1000) >> 10);
}

// convert time to string representation
// minimal length of string buffer `buf` should be 50
void xdag_time_to_string(time_t time, char* buf)
{
	struct tm tm;
	localtime_r(&time, &tm);
	strftime(buf, 50, "%Y-%m-%d %H:%M:%S", &tm);
}

time_t dag_get_xtimestamp(void)
{
	struct timeval tp;

	gettimeofday(&tp, 0);

	return (uint64_t)(unsigned long)tp.tv_sec << 10 | ((tp.tv_usec << 10) / 1000000);
}

uint64_t dag_get_time_ms(void)
{
	struct timeval tp;

	gettimeofday(&tp, 0);

	return (uint64_t)(unsigned long)tp.tv_sec * 1000 + tp.tv_usec / 1000;
}
