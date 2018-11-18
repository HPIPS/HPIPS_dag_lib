#pragma once

#include <time.h>
#include "time.h"
#include "block.h"
#include "system.h"

enum dag_states
{
#define xdag_state(n,s) XDAG_STATE_##n ,
#include "state.h"
#undef xdag_state
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
} g_xdag_stats;

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

class DAG_init
{
public:
	DAG_init();
	~DAG_init();

	/* the program state */
	int g_dag_state;

	/* is there command 'run' */
	int g_dag_run;

	/* 1 - the program works in a test network */
	int g_dag_testnet;

	/* coin token and program name */
	char *g_coinname, *g_progname;

#define coinname   g_coinname
	//defines if client runs as miner or pool
	int g_is_miner;

	//defines if mining is disabled (pool)
	int g_disable_mining;

	//Default type of the block header
	//Test network and main network have different types of the block headers, so blocks from different networks are incompatible
	enum dag_field_type g_block_header_type;

	int init(int argc, char **argv, int isGui);

	int dag_set_password_callback(int(*callback)(const char *prompt, char *buf, unsigned size));

	int(*g_dag_show_state)(const char *state, const char *balance, const char *address);
private:
	int g_is_pool = 0;
	time_t g_dag_xfer_last = 0;
	dag_stats g_dag_stats;
	dag_ext_stats g_dag_extstats;
	int g_disable_mining = 0;
	char g_pool_address[50] = { 0 };
};

