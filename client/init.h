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

/* 程序状态参数 */
extern int g_dag_state;

/* 有命令'运行'状态参数 */
extern int g_dag_run;

/* 1 - 程序在测试网络中工作 状态参数*/
extern int g_dag_testnet;

/* 硬币令牌和程序名称 */
extern char *g_coinname, *g_progname;

//定义客户端是作为矿工还是池运行
extern int g_is_miner;

//定义是否禁用挖掘（池）
extern int g_disable_mining;

//块头的默认类型
//测试网络和主网络具有不同类型的块头，因此来自不同网络的块是不兼容的
extern enum dag_field_type g_block_header_type;

extern int dag_init(int argc, char **argv, int isGui);

extern int dag_set_password_callback(int(*callback)(const char *prompt, char *buf, unsigned size));

extern int(*g_dag_show_state)(const char *state, const char *balance, const char *address);

#ifdef __cplusplus
};
#endif

#endif
