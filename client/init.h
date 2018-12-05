/* basic variables, T13.714-T14.582 $DVS:time$ */

#ifndef XDAG_MAIN_H
#define XDAG_MAIN_H

#include <time.h>
#include "time.h"
#include "block.h"
#include "system.h"

enum dag_states
{
#define dag_state(n,s) DAG_STATE_##n ,
#include "state.h"
#undef dag_state
};

extern struct dag_stats
{
	dag_diff_t difficulty, max_difficulty;
	uint64_t nblocks, total_nblocks;
	uint64_t nmain, total_nmain;
	uint32_t nhosts, total_nhosts;
	union {
		uint32_t reserved[2];
		uint64_t main_time;
	};
} g_dag_stats;

extern struct dag_ext_stats
{
	dag_diff_t hashrate_total[HASHRATE_LAST_MAX_TIME];
	dag_diff_t hashrate_ours[HASHRATE_LAST_MAX_TIME];
	xtime_t hashrate_last_time;
	uint64_t nnoref;
	uint64_t nextra;
	uint64_t nhashes;
	double hashrate_s;
	uint32_t nwaitsync;
	uint32_t cache_size;
	uint32_t cache_usage;
	double cache_hitrate;
	int use_orphan_hashtable;
} g_dag_extstats;

#ifdef __cplusplus
extern "C" {
#endif

/* ����״̬���� */
extern int g_dag_state;

/* ������'����'״̬���� */
extern int g_dag_run;

/* 1 - �����ڲ��������й��� ״̬����*/
extern int g_dag_testnet;

/* Ӳ�����ƺͳ������� */
extern char *g_coinname, *g_progname;

//����ͻ�������Ϊ�󹤻��ǳ�����
extern int g_is_miner;

//�����Ƿ�����ھ򣨳أ�
extern int g_disable_mining;

//��ͷ��Ĭ������
//�����������������в�ͬ���͵Ŀ�ͷ��������Բ�ͬ����Ŀ��ǲ����ݵ�
extern enum dag_field_type g_block_header_type;

extern int dag_init(int argc, char **argv, int isGui);

extern int dag_set_password_callback(int(*callback)(const char *prompt, char *buf, unsigned size));

extern int(*g_dag_show_state)(const char *state, const char *balance, const char *address);

#ifdef __cplusplus
};
#endif

#endif
