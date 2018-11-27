#ifndef XDAG_MINING_COMMON_H
#define XDAG_MINING_COMMON_H

#include <pthread.h>
#include "block.h"
#if defined(_WIN32) || defined(_WIN64)
#define poll WSAPoll
#else
#include <poll.h>
#endif

#define DATA_SIZE          (sizeof(struct xdag_field) / sizeof(uint32_t))
#define BLOCK_HEADER_WORD  0x3fca9e2bu

struct dag_pool_task {
	struct dag_field task[2], lastfield, minhash, nonce;
	xtime_t task_time;
	void *ctx0, *ctx;
};

#ifdef __cplusplus
extern "C" {
#endif
	
extern struct xdag_pool_task g_dag_pool_task[2];
extern uint64_t g_dag_pool_task_index; /* 全局变量被实例化 */

/* 互斥优化共享  */
extern void *g_ptr_share_mutex;

/* 程序作为一个池 */
extern int g_dag_pool;

extern const char *g_miner_address;

extern pthread_mutex_t g_share_mutex;

extern struct dfslib_crypt *g_crypt;

/* 池的初始化（g_xdag_pool=1）或将挖掘器连接到池（g_xdag_pool=0；pool_arg-pool参数ip:port[:CFG]；
矿工地址-矿工，如果指定*/
extern int dag_initialize_mining(const char *pool_arg, const char *miner_address);

//函数为任务设置最小份额
extern void dag_set_min_share(struct dag_pool_task *task, dag_hash_t last, dag_hash_t hash);

#ifdef __cplusplus
};
#endif
		
#endif
