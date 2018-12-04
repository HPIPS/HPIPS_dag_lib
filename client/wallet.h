#ifndef DAG_WALLET_H
#define DAG_WALLET_H

#include "block.h"

struct dag_public_key {
	void *key;
	uint64_t *pub; /* 最低比特包含奇偶校验 */
};

#ifdef __cplusplus
extern "C" {
#endif
	
/* 初始化钱包 */
extern int dag_wallet_init(void);

/* 生成一个新的关键字并设置为默认值，返回其索引 */
extern int dag_wallet_new_key(void);

/* 返回默认键，默认键的索引写入*NYKEY  */
extern struct dag_public_key *dag_wallet_default_key(int *n_key);

/* 返回密钥的数组 */
extern struct dag_public_key *dag_wallet_our_keys(int *pnkeys);

/* 用钱包完成工作 */
extern void dag_wallet_finish(void);

#ifdef __cplusplus
};
#endif
		
#endif
