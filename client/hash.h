/* hash function, T13.654-T13.775 $DVS:time$ */

#ifndef XDAG_HASH_H
#define XDAG_HASH_H

#include <stddef.h>
#include <stdint.h>

typedef uint64_t dag_hash_t[4];
typedef uint64_t dag_hashlow_t[3];

#ifdef __cplusplus
extern "C" {
#endif
	
extern void dag_hash(void *data, size_t size, dag_hash_t hash);

static inline int dag_cmphash(dag_hash_t l, dag_hash_t r)
{
	for(int i = 3; i >= 0; --i) if(l[i] != r[i]) return (l[i] < r[i] ? -1 : 1);
	return 0;
}

extern unsigned dag_hash_ctx_size(void);

extern void dag_hash_init(void *ctxv);

extern void dag_hash_update(void *ctxv, void *data, size_t size);

extern void dag_hash_final(void *ctxv, void *data, size_t size, dag_hash_t hash);

extern uint64_t dag_hash_final_multi(void *ctxv, uint64_t *nonce, int attempts, int step, dag_hash_t hash);

extern void dag_hash_get_state(void *ctxv, dag_hash_t state);

extern void dag_hash_set_state(void *ctxv, dag_hash_t state, size_t size);
	
#ifdef __cplusplus
};
#endif

#endif
