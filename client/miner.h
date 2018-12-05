/* pool and miner logic o_O, T13.744-T13.836 $DVS:time$ */

#ifndef XDAG_MINER_H
#define XDAG_MINER_H

#include <stdio.h>
#include "block.h"

#ifdef __cplusplus
extern "C" {
#endif
	
/* 一些挖掘线程 */
extern int g_dag_mining_threads;

/* 更改挖掘线程的数量 */
extern int dag_mining_start(int n_mining_threads);

/* 连接矿工到池的初始化 */
extern int dag_initialize_miner(const char *pool_address);

extern void *miner_net_thread(void *arg);

/* 通过池向网络发送块 */
extern int dag_send_block_via_pool(struct dag_block *block);

/* 从池列表中选择随机池 */
extern int dag_pick_pool(char *pool_address);

#ifdef __cplusplus
};
#endif
		
#endif
