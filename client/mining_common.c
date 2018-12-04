#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include "mining_common.h"
#include "miner.h"
#include "pool.h"
#include "../dus/programs/dfstools/source/dfslib/dfslib_crypt.h"

#define MINERS_PWD             "minersgonnamine"
#define SECTOR0_BASE           0x1947f3acu
#define SECTOR0_OFFSET         0x82e9d1b5u

/* 1 - program works as a pool */
int g_dag_pool = 0;

struct dag_pool_task g_dag_pool_task[2];
uint64_t g_dag_pool_task_index;

const char *g_miner_address;

pthread_mutex_t g_share_mutex = PTHREAD_MUTEX_INITIALIZER;

struct dfslib_crypt *g_crypt;

/* 互斥优化共享   */
void *g_ptr_share_mutex = &g_share_mutex;

static int crypt_start(void)
{
	struct dfslib_string str;
	uint32_t sector0[128];
	int i;

	g_crypt = malloc(sizeof(struct dfslib_crypt));
	if(!g_crypt) return -1;
	dfslib_crypt_set_password(g_crypt, dfslib_utf8_string(&str, MINERS_PWD, strlen(MINERS_PWD)));

	for(i = 0; i < 128; ++i) {
		sector0[i] = SECTOR0_BASE + i * SECTOR0_OFFSET;
	}

	for(i = 0; i < 128; ++i) {
		dfslib_crypt_set_sector0(g_crypt, sector0);
		dfslib_encrypt_sector(g_crypt, sector0, SECTOR0_BASE + i * SECTOR0_OFFSET);
	}

	return 0;
}

/* 池的初始化（g_xdag_pool=1）或将挖掘器连接到池（g_xdag_pool=0；pool_arg-pool参数ip:port[:CFG]；
矿工地址-矿工，如果指定*/
int dag_initialize_mining(const char *pool_arg, const char *miner_address)
{
	g_miner_address = miner_address;

	for(int i = 0; i < 2; ++i) {
		g_dag_pool_task[i].ctx0 = malloc(xdag_hash_ctx_size());
		g_dag_pool_task[i].ctx = malloc(xdag_hash_ctx_size());

		if(!g_dag_pool_task[i].ctx0 || !g_dag_pool_task[i].ctx) {
			return -1;
		}
	}

	if(!g_dag_pool && !pool_arg) return 0;

	if(crypt_start()) return -1;

	if(!g_dag_pool) {
		return dag_initialize_miner(pool_arg);
	} else {
		return dag_initialize_pool(pool_arg);
	}
}

//函数为任务设置最小份额
void dag_set_min_share(struct dag_pool_task *task, dag_hash_t last, dag_hash_t hash)
{
	if(dag_cmphash(hash, task->minhash.data) < 0) {
		pthread_mutex_lock(&g_share_mutex);

		if(dag_cmphash(hash, task->minhash.data) < 0) {
			memcpy(task->minhash.data, hash, sizeof(dag_hash_t));
			memcpy(task->lastfield.data, last, sizeof(dag_hash_t));
		}

		pthread_mutex_unlock(&g_share_mutex);
	}
}
