/* pool and miner logic o_O, T13.744-T13.836 $DVS:time$ */

#ifndef XDAG_MINER_H
#define XDAG_MINER_H

#include <stdio.h>
#include "block.h"

#ifdef __cplusplus
extern "C" {
#endif
	
/* һЩ�ھ��߳� */
extern int g_dag_mining_threads;

/* �����ھ��̵߳����� */
extern int dag_mining_start(int n_mining_threads);

/* ���ӿ󹤵��صĳ�ʼ�� */
extern int dag_initialize_miner(const char *pool_address);

extern void *miner_net_thread(void *arg);

/* ͨ���������緢�Ϳ� */
extern int dag_send_block_via_pool(struct dag_block *block);

/* �ӳ��б���ѡ������� */
extern int dag_pick_pool(char *pool_address);

#ifdef __cplusplus
};
#endif
		
#endif
