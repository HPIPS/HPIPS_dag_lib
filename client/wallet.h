/* кошелёк, T13.681-T13.726 $DVS:time$ */

#ifndef XDAG_WALLET_H
#define XDAG_WALLET_H

#include "block.h"

struct xdag_public_key {
	void *key;
	uint64_t *pub; /* lowest bit contains parity */
};

#ifdef __cplusplus
extern "C" {
#endif
	
/* initializes a wallet */
extern int xdag_wallet_init(void);

/* 生成一个新的关键字并设置为默认值，返回其索引 */
extern int dag_wallet_new_key(void);

/* returns a default key, the index of the default key is written to *n_key */
extern struct xdag_public_key *xdag_wallet_default_key(int *n_key);

/* returns an array of our keys */
extern struct xdag_public_key *xdag_wallet_our_keys(int *pnkeys);

/* completes work with wallet */
extern void xdag_wallet_finish(void);

#ifdef __cplusplus
};
#endif
		
#endif
