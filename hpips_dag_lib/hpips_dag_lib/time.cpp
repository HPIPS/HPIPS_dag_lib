#include "stdafx.h"
#include "time.h"
#include <sys/time.h>
#include "time.h"
#include "system.h"
#include "utils/utils.h"


time::time()
{
	time_t g_time_limit = DEF_TIME_LIMIT, g_xdag_era = DAG_MAIN_ERA;
}


time::~time()
{
}

time_t time::dag_main_time(void)
{
	return MAIN_TIME(dag_get_xtimestamp());
}

time_t time::dag_start_main_time(void)
{
	return MAIN_TIME(DAG_ERA);
}

int time::dag_time_init(void)
{
	if (g_dag_testnet) {
		g_dag_era = DAG_TEST_ERA;
	}

	return 1;
}

void time::dag_xtime_to_string(time_t time, char * buf)
{
	struct tm tm;
	char tmp[64] = { 0 };
	time_t t = time >> 10;
	localtime_r(&t, &tm);
	strftime(tmp, 60, "%Y-%m-%d %H:%M:%S", &tm);
	sprintf(buf, "%s.%03d", tmp, (int)((time & 0x3ff) * 1000) >> 10);
}

time_t time::dag_get_xtimestamp(void)
{
	struct timeval tp;

	gettimeofday(&tp, 0);

	return (uint64_t)(unsigned long)tp.tv_sec << 10 | ((tp.tv_usec << 10) / 1000000);
}

uint64_t time::dag_get_time_ms(void)
{
	struct timeval tp;

	gettimeofday(&tp, 0);

	return (uint64_t)(unsigned long)tp.tv_sec * 1000 + tp.tv_usec / 1000;
}
