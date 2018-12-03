/* addresses, T13.692-T13.692 $DVS:time$ */

#ifndef XDAG_ADDRESS_H
#define XDAG_ADDRESS_H

#include "hash.h"

#ifdef __cplusplus
extern "C" {
#endif
	
/* 初始化地址模块 */
extern int dag_address_init(void);

/* 转换address到hash */
extern int dag_address2hash(const char *address, dag_hash_t hash);

/* 转换hash到address */
extern void dag_hash2address(const dag_hash_t hash, char *address);
	
#ifdef __cplusplus
};
#endif

#endif
