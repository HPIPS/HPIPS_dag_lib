/* кошелёк, T13.681-T13.788 $DVS:time$ */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "crypt.h"
#include "wallet.h"
#include "init.h"
#include "transport.h"
#include "utils/log.h"
#include "utils/utils.h"

#define WALLET_FILE (g_xdag_testnet ? "wallet-testnet.dat" : "wallet.dat")

struct key_internal {
	xdag_hash_t pub, priv;
	void *key;
	struct key_internal *prev;
	uint8_t pub_bit;
};

static struct key_internal *def_key = 0;
static struct xdag_public_key *keys_arr = 0;
static pthread_mutex_t wallet_mutex = PTHREAD_MUTEX_INITIALIZER;
int nkeys = 0, maxnkeys = 0;

static int add_key(dag_hash_t priv)
{
	struct key_internal *k = malloc(sizeof(struct key_internal));

	if (!k) return -1;

	pthread_mutex_lock(&wallet_mutex);

	if (priv) {
		memcpy(k->priv, priv, sizeof(dag_hash_t));
		k->key = xdag_private_to_key(k->priv, k->pub, &k->pub_bit);
	} else {
		FILE *f;
		uint32_t priv32[sizeof(dag_hash_t) / sizeof(uint32_t)];

		k->key = xdag_create_key(k->priv, k->pub, &k->pub_bit);
		
		f = xdag_open_file(WALLET_FILE, "ab");
		if (!f) goto fail;
		
		memcpy(priv32, k->priv, sizeof(dag_hash_t));
		
		xdag_user_crypt_action(priv32, nkeys, sizeof(dag_hash_t) / sizeof(uint32_t), 1);
		
		if (fwrite(priv32, sizeof(dag_hash_t), 1, f) != 1) {
			xdag_close_file(f);
			goto fail;
		}

		xdag_close_file(f);
	}

	if (!k->key) goto fail;
	
	k->prev = def_key;
	def_key = k;
	
	if (nkeys == maxnkeys) {
		struct dag_public_key *newarr = (struct dag_public_key *)
			realloc(keys_arr, ((maxnkeys | 0xff) + 1) * sizeof(struct dag_public_key));
		if (!newarr) goto fail;
		
		maxnkeys |= 0xff;
		maxnkeys++;
		keys_arr = newarr;
	}

	keys_arr[nkeys].key = k->key;
	keys_arr[nkeys].pub = (uint64_t*)((uintptr_t)&k->pub | k->pub_bit);

	xdag_debug("Key %2d: priv=[%s] pub=[%02x:%s]", nkeys,
					dag_log_hash(k->priv), 0x02 + k->pub_bit,  dag_log_hash(k->pub));
	
	nkeys++;
	
	pthread_mutex_unlock(&wallet_mutex);
	
	return 0;
 
fail:
	pthread_mutex_unlock(&wallet_mutex);
	free(k);
	return -1;
}

/* 生成一个新的关键字并设置为默认值，返回其索引 */
int dag_wallet_new_key(void)
{
	int res = add_key(0);

	if (!res)
		res = nkeys - 1;

	return res;
}

/* 初始化钱包 */
int dag_wallet_init(void)
{
	uint32_t priv32[sizeof(dag_hash_t) / sizeof(uint32_t)];
	dag_hash_t priv;
	FILE *f = dag_open_file(WALLET_FILE, "rb");
	int n;

	if (!f) {
		if (add_key(0)) return -1;
		
		f = dag_open_file(WALLET_FILE, "r");
		if (!f) return -1;
		
		fread(priv32, sizeof(dag_hash_t), 1, f);
		
		n = 1;
	} else {
		n = 0;
	}

	while (fread(priv32, sizeof(dag_hash_t), 1, f) == 1) {
		dag_user_crypt_action(priv32, n++, sizeof(dag_hash_t) / sizeof(uint32_t), 2);
		memcpy(priv, priv32, sizeof(dag_hash_t));
		add_key(priv);
	}

	dag_close_file(f);
	
	return 0;
}

/* 返回默认键，默认键的索引写入*NYKEY*/
struct dag_public_key *dag_wallet_default_key(int *n_key)
{
	if (nkeys) {
		if (n_key) {
			*n_key = nkeys - 1;
			return keys_arr + nkeys - 1;
		}
	}

	return 0;
}

/* 返回密钥的数组 */
struct dag_public_key *dag_wallet_our_keys(int *pnkeys)
{
	*pnkeys = nkeys;

	return keys_arr;
}

/* 用钱包完成工作 */
void dag_wallet_finish(void)
{
	pthread_mutex_lock(&wallet_mutex);
}
