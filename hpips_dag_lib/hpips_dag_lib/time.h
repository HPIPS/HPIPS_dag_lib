#pragma once
#include <stdint.h>
#include "types.h"

/* the maximum period of time for which blocks are requested, not their amounts */
#define REQUEST_BLOCKS_MAX_TIME (1 << 20)

#define HASHRATE_LAST_MAX_TIME  (64 * 4) // numbers of main blocks in about 4H, to calculate the pool and network mean hashrate
#define DEF_TIME_LIMIT          0 // (MAIN_CHAIN_PERIOD / 2)
#define MAIN_CHAIN_PERIOD       (64 << 10)
#define MAIN_TIME(t)            ((t) >> 16)
#define XDAG_TEST_ERA           0x16900000000ll
#define XDAG_MAIN_ERA           0x16940000000ll
#define XDAG_ERA                g_xdag_era
#define MAX_TIME_NMAIN_STALLED  (1 << 10)

class time
{
public:
	time();
	~time();
	time_t g_dag_era;
	//����ʱ��������������������Ϊ64�볤��
	time_t dag_main_time(void);
	//���ض�Ӧ�����翪ʼ��ʱ����������
	time_t dag_start_main_time(void);
	//��ʼ��ʱ��ģ��
	int dag_time_init(void);
	// ת��time_t���ַ�����ʾ
	// �ַ�����������BUF������С����Ӧ����50��
	void dag_xtime_to_string(time_t time, char *buf);

	time_t dag_get_xtimestamp(void);
	uint64_t dag_get_time_ms(void);
private:
	int g_dag_testnet;
};

