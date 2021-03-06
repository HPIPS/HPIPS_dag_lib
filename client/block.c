/* block processing, T13.654-T14.618 $DVS:time$ */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <math.h>
#include "system.h"
#include "../ldus/source/include/ldus/rbtree.h"
#include "block.h"
#include "crypt.h"
#include "wallet.h"
#include "storage.h"
#include "transport.h"
#include "utils/log.h"
#include "init.h"
#include "sync.h"
#include "pool.h"
#include "miner.h"
#include "memory.h"
#include "address.h"
#include "commands.h"
#include "utils/utils.h"
#include "utils/moving_statistics/moving_average.h"
#include "mining_common.h"
#include "time.h"
#include "math.h"
#include "utils/atomic.h"

#define MAX_WAITING_MAIN        1
#define MAIN_START_AMOUNT       (1ll << 42)
#define MAIN_BIG_PERIOD_LOG     21
#define MAX_LINKS               15
#define MAKE_BLOCK_PERIOD       13

#define CACHE			1
#define CACHE_MAX_SIZE		600000
#define CACHE_MAX_SAMPLES	100
#define ORPHAN_HASH_SIZE	2
#define MAX_ALLOWED_EXTRA	0x10000

struct block_backrefs;
struct orphan_block;
struct block_internal_index;

struct block_internal {
	union {
		struct ldus_rbtree node;
		struct block_internal_index *index;
	};
	dag_hash_t hash;
	dag_diff_t difficulty;
	dag_amount_t amount, linkamount[MAX_LINKS], fee;
	xtime_t time;
	uint64_t storage_pos;
	union {
		struct block_internal *ref;
		struct orphan_block *oref;
	};
	struct block_internal *link[MAX_LINKS];
	atomic_uintptr_t backrefs;
	atomic_uintptr_t remark;
	uint16_t flags, in_mask, n_our_key;
	uint8_t nlinks:4, max_diff_link:4, reserved;
};

struct block_internal_index {
	struct ldus_rbtree node;
	dag_hash_t hash;
	struct block_internal *bi;
};

#define N_BACKREFS      (sizeof(struct block_internal) / sizeof(struct block_internal *) - 1)

struct block_backrefs {
	struct block_internal *backrefs[N_BACKREFS];
	struct block_backrefs *next;
};

#define ourprev link[MAX_LINKS - 2]
#define ournext link[MAX_LINKS - 1]

struct cache_block {
	struct ldus_rbtree node;
	dag_hash_t hash;
	struct dag_block block;
	struct cache_block *next;
};

struct orphan_block {
	struct block_internal *orphan_bi;
	struct orphan_block *next;
	struct orphan_block *prev;
	struct dag_block block[0];
};

enum orphan_remove_actions {
	ORPHAN_REMOVE_NORMAL,
	ORPHAN_REMOVE_REUSE,
	ORPHAN_REMOVE_EXTRA
};

#define get_orphan_index(bi)      (!!((bi)->flags & BI_EXTRA))

int g_bi_index_enable = 1, g_block_production_on;
static pthread_mutex_t g_create_block_mutex = PTHREAD_MUTEX_INITIALIZER;
static dag_amount_t g_balance = 0;
extern xtime_t g_time_limit;
static struct ldus_rbtree *root = 0, *cache_root = 0;
static struct block_internal *volatile top_main_chain = 0, *volatile pretop_main_chain = 0;
static struct block_internal *ourfirst = 0, *ourlast = 0;
static struct cache_block *cache_first = NULL, *cache_last = NULL;
static pthread_mutex_t block_mutex;
static pthread_mutex_t rbtree_mutex;
//TODO: this variable duplicates existing global variable g_is_pool. Probably should be removed
static int g_light_mode = 0;
static uint32_t cache_bounded_counter = 0;
static struct orphan_block *g_orphan_first[ORPHAN_HASH_SIZE], *g_orphan_last[ORPHAN_HASH_SIZE];

//functions
void cache_retarget(int32_t, int32_t);
void cache_add(struct dag_block*, dag_hash_t);
int32_t check_signature_out_cached(struct block_internal*, struct dag_public_key*, const int, int32_t*, int32_t*);
int32_t check_signature_out(struct block_internal*, struct dag_public_key*, const int);
static int32_t find_and_verify_signature_out(struct dag_block*, struct dag_public_key*, const int);
int do_mining(struct dag_block *block, struct block_internal **pretop, xtime_t send_time);
void remove_orphan(struct block_internal*,int);
void add_orphan(struct block_internal*,struct dag_block*);
static inline size_t remark_acceptance(dag_remark_t);
static int add_remark_bi(struct block_internal*, dag_remark_t);
static void add_backref(struct block_internal*, struct block_internal*);
static inline int get_nfield(struct dag_block*, int);
static inline const char* get_remark(struct block_internal*);
static int load_remark(struct block_internal*);
static void order_ourblocks_by_amount(struct block_internal *bi);
static inline void add_ourblock(struct block_internal *nodeBlock);
static inline void remove_ourblock(struct block_internal *nodeBlock);
void *add_block_callback(void *block, void *data);
extern void *sync_thread(void *arg);

static inline int lessthan(struct ldus_rbtree *l, struct ldus_rbtree *r)
{
	return memcmp(l + 1, r + 1, 24) < 0;
}

ldus_rbtree_define_prefix(lessthan, static inline, )

static inline struct block_internal *block_by_hash(const dag_hashlow_t hash)
{
	if(g_bi_index_enable) {
		struct block_internal_index *index;
		pthread_mutex_lock(&rbtree_mutex);
		index = (struct block_internal_index *)ldus_rbtree_find(root, (struct ldus_rbtree *)hash - 1);
		pthread_mutex_unlock(&rbtree_mutex);
		return index ? index->bi : NULL;
	} else {
		struct block_internal *bi;
		pthread_mutex_lock(&rbtree_mutex);
		bi = (struct block_internal *)ldus_rbtree_find(root, (struct ldus_rbtree *)hash - 1);
		pthread_mutex_unlock(&rbtree_mutex);
		return bi;
	}
}

static inline struct cache_block *cache_block_by_hash(const dag_hashlow_t hash)
{
	return (struct cache_block *)ldus_rbtree_find(cache_root, (struct ldus_rbtree *)hash - 1);
}


static void log_block(const char *mess, dag_hash_t h, xtime_t t, uint64_t pos)
{
	/* Do not log blocks as we are loading from local storage */
	if(g_dag_state != DAG_STATE_LOAD) {
		dag_info("%s: %016llx%016llx%016llx%016llx t=%llx pos=%llx", mess,
			((uint64_t*)h)[3], ((uint64_t*)h)[2], ((uint64_t*)h)[1], ((uint64_t*)h)[0], t, pos);
	}
}

static inline void accept_amount(struct block_internal *bi, dag_amount_t sum)
{
	if (!sum) {
		return;
	}

	bi->amount += sum;
	if (bi->flags & BI_OURS) {
		g_balance += sum;
		order_ourblocks_by_amount(bi);
	}
}

static uint64_t apply_block(struct block_internal *bi)
{
	dag_amount_t sum_in, sum_out;

	if (bi->flags & BI_MAIN_REF) {
		return -1l;
	}

	bi->flags |= BI_MAIN_REF;

	for (int i = 0; i < bi->nlinks; ++i) {
		dag_amount_t ref_amount = apply_block(bi->link[i]);
		if (ref_amount == -1l) {
			continue;
		}
		bi->link[i]->ref = bi;
		if (bi->amount + ref_amount >= bi->amount) {
			accept_amount(bi, ref_amount);
		}
	}

	sum_in = 0, sum_out = bi->fee;

	for (int i = 0; i < bi->nlinks; ++i) {
		if (1 << i & bi->in_mask) {
			if (bi->link[i]->amount < bi->linkamount[i]) {
				return 0;
			}
			if (sum_in + bi->linkamount[i] < sum_in) {
				return 0;
			}
			sum_in += bi->linkamount[i];
		} else {
			if (sum_out + bi->linkamount[i] < sum_out) {
				return 0;
			}
			sum_out += bi->linkamount[i];
		}
	}

	if (sum_in + bi->amount < sum_in || sum_in + bi->amount < sum_out) {
		return 0;
	}

	for (int i = 0; i < bi->nlinks; ++i) {
		if (1 << i & bi->in_mask) {
			accept_amount(bi->link[i], (dag_amount_t)0 - bi->linkamount[i]);
		} else {
			accept_amount(bi->link[i], bi->linkamount[i]);
		}
	}

	accept_amount(bi, sum_in - sum_out);
	bi->flags |= BI_APPLIED;

	return bi->fee;
}

static uint64_t unapply_block(struct block_internal *bi)
{
	int i;

	if (bi->flags & BI_APPLIED) {
		dag_amount_t sum = bi->fee;

		for (i = 0; i < bi->nlinks; ++i) {
			if (1 << i & bi->in_mask) {
				accept_amount(bi->link[i], bi->linkamount[i]);
				sum -= bi->linkamount[i];
			} else {
				accept_amount(bi->link[i], (dag_amount_t)0 - bi->linkamount[i]);
				sum += bi->linkamount[i];
			}
		}

		accept_amount(bi, sum);
		bi->flags &= ~BI_APPLIED;
	}

	bi->flags &= ~BI_MAIN_REF;
	bi->ref = 0;

	for (i = 0; i < bi->nlinks; ++i) {
		if (bi->link[i]->ref == bi && bi->link[i]->flags & BI_MAIN_REF) {
			accept_amount(bi, unapply_block(bi->link[i]));
		}
	}

	return (dag_amount_t)0 - bi->fee;
}

// 按指定的主要块数计算当前供应量
dag_amount_t dag_get_supply(uint64_t nmain)
{
	dag_amount_t res = 0, amount = MAIN_START_AMOUNT;

	while (nmain >> MAIN_BIG_PERIOD_LOG) {
		res += (1l << MAIN_BIG_PERIOD_LOG) * amount;
		nmain -= 1l << MAIN_BIG_PERIOD_LOG;
		amount >>= 1;
	}
	res += nmain * amount;
	return res;
}

static void set_main(struct block_internal *m)
{
	dag_amount_t amount = MAIN_START_AMOUNT >> (g_dag_stats.nmain >> MAIN_BIG_PERIOD_LOG);

	m->flags |= BI_MAIN;
	accept_amount(m, amount);
	g_dag_stats.nmain++;

	if (g_dag_stats.nmain > g_dag_stats.total_nmain) {
		g_dag_stats.total_nmain = g_dag_stats.nmain;
	}

	accept_amount(m, apply_block(m));
	m->ref = m;
	log_block((m->flags & BI_OURS ? "MAIN +" : "MAIN  "), m->hash, m->time, m->storage_pos);
}

static void unset_main(struct block_internal *m)
{
	g_dag_stats.nmain--;
	g_dag_stats.total_nmain--;
	dag_amount_t amount = MAIN_START_AMOUNT >> (g_dag_stats.nmain >> MAIN_BIG_PERIOD_LOG);
	m->flags &= ~BI_MAIN;
	accept_amount(m, (dag_amount_t)0 - amount);
	accept_amount(m, unapply_block(m));
	log_block("UNMAIN", m->hash, m->time, m->storage_pos);
}

static void check_new_main(void)
{
	struct block_internal *b, *p = 0;
	int i;

	for (b = top_main_chain, i = 0; b && !(b->flags & BI_MAIN); b = b->link[b->max_diff_link]) {
		if (b->flags & BI_MAIN_CHAIN) {
			p = b;
			++i;
		}
	}

	if (p && (p->flags & BI_REF) && i > MAX_WAITING_MAIN && dag_get_xtimestamp() >= p->time + 2 * 1024) {
		set_main(p);
	}
}

static void unwind_main(struct block_internal *b)
{
	for (struct block_internal *t = top_main_chain; t && t != b; t = t->link[t->max_diff_link]) {
		t->flags &= ~BI_MAIN_CHAIN;
		if (t->flags & BI_MAIN) {
			unset_main(t);
		}
	}
}

static inline void hash_for_signature(struct dag_block b[2], const struct dag_public_key *key, dag_hash_t hash)
{
	memcpy((uint8_t*)(b + 1) + 1, (void*)((uintptr_t)key->pub & ~1l), sizeof(dag_hash_t));

	*(uint8_t*)(b + 1) = ((uintptr_t)key->pub & 1) | 0x02;

	dag_hash(b, sizeof(struct dag_block) + sizeof(dag_hash_t) + 1, hash);

	dag_debug("Hash  : hash=[%s] data=[%s]", dag_log_hash(hash),
		dag_log_array(b, sizeof(struct dag_block) + sizeof(dag_hash_t) + 1));
}

// returns a number of public key from 'keys' array with lengh 'keysLength', which conforms to the signature starting from field signo_r of the block b
// returns -1 if nothing is found
static int valid_signature(const struct dag_block *b, int signo_r, int keysLength, struct dag_public_key *keys)
{
	struct dag_block buf[2];
	dag_hash_t hash;
	int i, signo_s = -1;

	memcpy(buf, b, sizeof(struct dag_block));

	for(i = signo_r; i < DAG_BLOCK_FIELDS; ++i) {
		if(dag_type(b, i) == DAG_FIELD_SIGN_IN || dag_type(b, i) == DAG_FIELD_SIGN_OUT) {
			memset(&buf[0].field[i], 0, sizeof(struct dag_field));
			if(i > signo_r && signo_s < 0 && dag_type(b, i) == dag_type(b, signo_r)) {
				signo_s = i;
			}
		}
	}

	if(signo_s >= 0) {
		for(i = 0; i < keysLength; ++i) {
			hash_for_signature(buf, keys + i, hash);

#if USE_OPTIMIZED_EC == 1
			if(!dag_verify_signature_optimized_ec(keys[i].pub, hash, b->field[signo_r].data, b->field[signo_s].data)) {
#elif USE_OPTIMIZED_EC == 2
			int res1 = !xdag_verify_signature_optimized_ec(keys[i].pub, hash, b->field[signo_r].data, b->field[signo_s].data);
			int res2 = !xdag_verify_signature(keys[i].key, hash, b->field[signo_r].data, b->field[signo_s].data);
			if(res1 != res2) {
				dag_warn("Different result between openssl and secp256k1: res openssl=%2d res secp256k1=%2d key parity bit = %ld key=[%s] hash=[%s] r=[%s], s=[%s]",
					res2, res1, ((uintptr_t)keys[i].pub & 1), dag_log_hash((uint64_t*)((uintptr_t)keys[i].pub & ~1l)),
					dag_log_hash(hash), dag_log_hash(b->field[signo_r].data), dag_log_hash(b->field[signo_s].data));
			}
			if(res2) {
#else
			if(!xdag_verify_signature(keys[i].key, hash, b->field[signo_r].data, b->field[signo_s].data)) {
#endif
				return i;
			}
		}
	}

	return -1;
}

static int remove_index(struct block_internal *bi)
{
	if(g_bi_index_enable) {
		pthread_mutex_lock(&rbtree_mutex);
		ldus_rbtree_remove(&root, &bi->index->node);
		pthread_mutex_unlock(&rbtree_mutex);
		free(bi->index);
		bi->index = NULL;
	} else {
		pthread_mutex_lock(&rbtree_mutex);
		ldus_rbtree_remove(&root, &bi->node);
		pthread_mutex_unlock(&rbtree_mutex);
	}
	return 0;
}

static int insert_index(struct block_internal *bi)
{
	if(g_bi_index_enable) {
		struct block_internal_index *index = (struct block_internal_index *)malloc(sizeof(struct block_internal_index));
		if(!index) {
			dag_err("block index malloc failed. [func: add_block_nolock]");
			return -1;
		}
		memset(index, 0, sizeof(struct block_internal_index));
		memcpy(index->hash, bi->hash, sizeof(dag_hash_t));
		index->bi = bi;
		bi->index = index;

		pthread_mutex_lock(&rbtree_mutex);
		ldus_rbtree_insert(&root, &index->node);
		pthread_mutex_unlock(&rbtree_mutex);
	} else {
		pthread_mutex_lock(&rbtree_mutex);
		ldus_rbtree_insert(&root, &bi->node);
		pthread_mutex_unlock(&rbtree_mutex);
	}
	return 0;
}

#define set_pretop(b) if ((b) && MAIN_TIME((b)->time) < MAIN_TIME(timestamp) && \
		(!pretop_main_chain || dag_diff_gt((b)->difficulty, pretop_main_chain->difficulty))) { \
		pretop_main_chain = (b); \
		log_block("Pretop", (b)->hash, (b)->time, (b)->storage_pos); \
}

/* checks and adds a new block to the storage
 * returns:
 *		>0 = block was added
 *		0  = block exists
 *		<0 = error
 */
static int add_block_nolock(struct dag_block *newBlock, xtime_t limit)
{
	const xtime_t timestamp = dag_get_xtimestamp();
	uint64_t sum_in = 0, sum_out = 0, *psum = NULL;
	const uint64_t transportHeader = newBlock->field[0].transport_header;
	struct dag_public_key public_keys[16], *our_keys = 0;
	int i = 0, j = 0;
	int keysCount = 0, ourKeysCount = 0;
	int signInCount = 0, signOutCount = 0;
	int signinmask = 0, signoutmask = 0;
	int inmask = 0, outmask = 0, remark_index = 0;
	int verified_keys_mask = 0, err = 0, type = 0;
	struct block_internal tmpNodeBlock, *blockRef = NULL, *blockRef0 = NULL;
	struct block_internal* blockRefs[DAG_BLOCK_FIELDS-1]= {0};
	dag_diff_t diff0, diff;
	int32_t cache_hit = 0, cache_miss = 0;

	memset(&tmpNodeBlock, 0, sizeof(struct block_internal));
	newBlock->field[0].transport_header = 0;
	dag_hash(newBlock, sizeof(struct dag_block), tmpNodeBlock.hash);

	if(block_by_hash(tmpNodeBlock.hash)) return 0;

	if(dag_type(newBlock, 0) != g_block_header_type) {
		i = dag_type(newBlock, 0);
		err = 1;
		goto end;
	}

	tmpNodeBlock.time = newBlock->field[0].time;

	if(tmpNodeBlock.time > timestamp + MAIN_CHAIN_PERIOD / 4 || tmpNodeBlock.time < g_dag_era
		|| (limit && timestamp - tmpNodeBlock.time > limit)) {
		i = 0;
		err = 2;
		goto end;
	}

	for(i = 1; i < DAG_BLOCK_FIELDS; ++i) {
		switch((type = dag_type(newBlock, i))) {
			case DAG_FIELD_NONCE:
				break;
			case DAG_FIELD_IN:
				inmask |= 1 << i;
				break;
			case DAG_FIELD_OUT:
				outmask |= 1 << i;
				break;
			case DAG_FIELD_SIGN_IN:
				if(++signInCount & 1) {
					signinmask |= 1 << i;
				}
				break;
			case DAG_FIELD_SIGN_OUT:
				if(++signOutCount & 1) {
					signoutmask |= 1 << i;
				}
				break;
			case DAG_FIELD_PUBLIC_KEY_0:
			case DAG_FIELD_PUBLIC_KEY_1:
				if((public_keys[keysCount].key = dag_public_to_key(newBlock->field[i].data, type - DAG_FIELD_PUBLIC_KEY_0))) {
					public_keys[keysCount++].pub = (uint64_t*)((uintptr_t)&newBlock->field[i].data | (type - DAG_FIELD_PUBLIC_KEY_0));
				}
				break;

			case DAG_FIELD_REMARK:
				tmpNodeBlock.flags |= BI_REMARK;
				remark_index = i;
				break;
			case DAG_FIELD_RESERVE1:
			case DAG_FIELD_RESERVE2:
			case DAG_FIELD_RESERVE3:
			case DAG_FIELD_RESERVE4:
			case DAG_FIELD_RESERVE5:
			case DAG_FIELD_RESERVE6:
				break;
			default:
				err = 3;
				goto end;
		}
	}

	if(g_light_mode) {
		outmask = 0;
	}

	if(signOutCount & 1) {
		i = signOutCount;
		err = 4;
		goto end;
	}

	/* check remark */
	if(tmpNodeBlock.flags & BI_REMARK) {
		if(!remark_acceptance(newBlock->field[remark_index].remark)) {
			err = 0xC;
			goto end;
		}
	}

	/* if not read from storage and timestamp is ...ffff and last field is nonce then the block is extra */
	if (!g_light_mode && (transportHeader & (sizeof(struct dag_block) - 1))
			&& (tmpNodeBlock.time & (MAIN_CHAIN_PERIOD - 1)) == (MAIN_CHAIN_PERIOD - 1)
			&& (signinmask & 1 << (DAG_BLOCK_FIELDS - 1))) {
		tmpNodeBlock.flags |= BI_EXTRA;
	}

	for(i = 1; i < DAG_BLOCK_FIELDS; ++i) {
		if(1 << i & (inmask | outmask)) {
			blockRefs[i-1] = block_by_hash(newBlock->field[i].hash);
			if(!blockRefs[i-1]) {
				err = 5;
				goto end;
			}
			if(blockRefs[i-1]->time >= tmpNodeBlock.time) {
				err = 6;
				goto end;
			}
			if(tmpNodeBlock.nlinks >= MAX_LINKS) {
				err = 7;
				goto end;
			}
		}
	}

	if(!g_light_mode) {
		check_new_main();
	}

	if(signOutCount) {
		our_keys = dag_wallet_our_keys(&ourKeysCount);
	}

	for(i = 1; i < DAG_BLOCK_FIELDS; ++i) {
		if(1 << i & (signinmask | signoutmask)) {
			int keyNumber = valid_signature(newBlock, i, keysCount, public_keys);
			if(keyNumber >= 0) {
				verified_keys_mask |= 1 << keyNumber;
			}
			if(1 << i & signoutmask && !(tmpNodeBlock.flags & BI_OURS) && (keyNumber = valid_signature(newBlock, i, ourKeysCount, our_keys)) >= 0) {
				tmpNodeBlock.flags |= BI_OURS;
				tmpNodeBlock.n_our_key = keyNumber;
			}
		}
	}

	for(i = j = 0; i < keysCount; ++i) {
		if(1 << i & verified_keys_mask) {
			if(i != j) {
				dag_free_key(public_keys[j].key);
			}
			memcpy(public_keys + j++, public_keys + i, sizeof(struct dag_public_key));
		}
	}

	keysCount = j;
	tmpNodeBlock.difficulty = diff0 = xdag_hash_difficulty(tmpNodeBlock.hash);
	sum_out += newBlock->field[0].amount;
	tmpNodeBlock.fee = newBlock->field[0].amount;
	if (tmpNodeBlock.fee) {
		tmpNodeBlock.flags &= ~BI_EXTRA;
	}

	for(i = 1; i < DAG_BLOCK_FIELDS; ++i) {
		if(1 << i & (inmask | outmask)) {
			blockRef = blockRefs[i-1];
			if(1 << i & inmask) {
				if(newBlock->field[i].amount) {
					int32_t res = 1;
					if(CACHE) {
						res = check_signature_out_cached(blockRef, public_keys, keysCount, &cache_hit, &cache_miss);
					} else {
						res = check_signature_out(blockRef, public_keys, keysCount);
					}
					if(res) {
						err = res;
						goto end;
					}

				}
				psum = &sum_in;
				tmpNodeBlock.in_mask |= 1 << tmpNodeBlock.nlinks;
			} else {
				psum = &sum_out;
			}

			if (newBlock->field[i].amount) {
				tmpNodeBlock.flags &= ~BI_EXTRA;
			}

			if(*psum + newBlock->field[i].amount < *psum) {
				err = 0xA;
				goto end;
			}

			*psum += newBlock->field[i].amount;
			tmpNodeBlock.link[tmpNodeBlock.nlinks] = blockRef;
			tmpNodeBlock.linkamount[tmpNodeBlock.nlinks] = newBlock->field[i].amount;

			if(MAIN_TIME(blockRef->time) < MAIN_TIME(tmpNodeBlock.time)) {
				diff = xdag_diff_add(diff0, blockRef->difficulty);
			} else {
				diff = blockRef->difficulty;

				while(blockRef && MAIN_TIME(blockRef->time) == MAIN_TIME(tmpNodeBlock.time)) {
					blockRef = blockRef->link[blockRef->max_diff_link];
				}
				if(blockRef && dag_diff_gt(xdag_diff_add(diff0, blockRef->difficulty), diff)) {
					diff = xdag_diff_add(diff0, blockRef->difficulty);
				}
			}

			if(dag_diff_gt(diff, tmpNodeBlock.difficulty)) {
				tmpNodeBlock.difficulty = diff;
				tmpNodeBlock.max_diff_link = tmpNodeBlock.nlinks;
			}

			tmpNodeBlock.nlinks++;
		}
	}

	if(CACHE) {
		cache_retarget(cache_hit, cache_miss);
	}

	if(tmpNodeBlock.in_mask ? sum_in < sum_out : sum_out != newBlock->field[0].amount) {
		err = 0xB;
		goto end;
	}

	struct block_internal *nodeBlock;
	if (g_dag_extstats.nextra > MAX_ALLOWED_EXTRA
			&& (g_dag_state == DAG_STATE_SYNC || g_dag_state == DAG_STATE_STST)) {
		/* if too many extra blocks then reuse the oldest */
		nodeBlock = g_orphan_first[1]->orphan_bi;
		remove_orphan(nodeBlock, ORPHAN_REMOVE_REUSE);
		remove_index(nodeBlock);
		if (g_dag_stats.nblocks-- == g_dag_stats.total_nblocks)
			g_dag_stats.total_nblocks--;
		if (nodeBlock->flags & BI_OURS) {
			remove_ourblock(nodeBlock);
		}
	} else {
		nodeBlock = dag_malloc(sizeof(struct block_internal));
	}
	if(!nodeBlock) {
		err = 0xC;
		goto end;
	}

	if(CACHE && signOutCount) {
		cache_add(newBlock, tmpNodeBlock.hash);
	}

	if(!(transportHeader & (sizeof(struct dag_block) - 1))) {
		tmpNodeBlock.storage_pos = transportHeader;
	} else if (!(tmpNodeBlock.flags & BI_EXTRA)) {
		tmpNodeBlock.storage_pos = dag_storage_save(newBlock);
	} else {
		/* do not store extra block right now */
		tmpNodeBlock.storage_pos = -2l;
	}

	memcpy(nodeBlock, &tmpNodeBlock, sizeof(struct block_internal));
	atomic_init_uintptr(&nodeBlock->backrefs, (uintptr_t)NULL);
	if(nodeBlock->flags & BI_REMARK){
		atomic_init_uintptr(&nodeBlock->remark, (uintptr_t)NULL);
	}

	if(!insert_index(nodeBlock)) {
		g_dag_stats.nblocks++;
	} else {
		err = 0xC;
		goto end;
	}

	if(g_dag_stats.nblocks > g_dag_stats.total_nblocks) {
		g_dag_stats.total_nblocks = g_dag_stats.nblocks;
	}

	set_pretop(nodeBlock);
	set_pretop(top_main_chain);

	if(dag_diff_gt(tmpNodeBlock.difficulty, g_dag_stats.difficulty)) {
		/* Only log this if we are NOT loading state */
		if(g_dag_state != DAG_STATE_LOAD)
			dag_info("Diff  : %llx%016llx (+%llx%016llx)", xdag_diff_args(tmpNodeBlock.difficulty), xdag_diff_args(diff0));

		for(blockRef = nodeBlock, blockRef0 = 0; blockRef && !(blockRef->flags & BI_MAIN_CHAIN); blockRef = blockRef->link[blockRef->max_diff_link]) {
			if((!blockRef->link[blockRef->max_diff_link] || dag_diff_gt(blockRef->difficulty, blockRef->link[blockRef->max_diff_link]->difficulty))
				&& (!blockRef0 || MAIN_TIME(blockRef0->time) > MAIN_TIME(blockRef->time))) {
				blockRef->flags |= BI_MAIN_CHAIN;
				blockRef0 = blockRef;
			}
		}

		if(blockRef && blockRef0 && blockRef != blockRef0 && MAIN_TIME(blockRef->time) == MAIN_TIME(blockRef0->time)) {
			blockRef = blockRef->link[blockRef->max_diff_link];
		}

		unwind_main(blockRef);
		top_main_chain = nodeBlock;
		g_dag_stats.difficulty = tmpNodeBlock.difficulty;

		if(dag_diff_gt(g_dag_stats.difficulty, g_dag_stats.max_difficulty)) {
			g_dag_stats.max_difficulty = g_dag_stats.difficulty;
		}

		err = -1;
	} else if (tmpNodeBlock.flags & BI_EXTRA) {
		err = 0;
	} else {
		err = -1;
	}

	if(tmpNodeBlock.flags & BI_OURS) {
		add_ourblock(nodeBlock);
	}

	for(i = 0; i < tmpNodeBlock.nlinks; ++i) {
		remove_orphan(tmpNodeBlock.link[i],
				tmpNodeBlock.flags & BI_EXTRA ? ORPHAN_REMOVE_EXTRA : ORPHAN_REMOVE_NORMAL);

		if(tmpNodeBlock.linkamount[i]) {
			blockRef = tmpNodeBlock.link[i];
			add_backref(blockRef, nodeBlock);
		}
	}
	
	add_orphan(nodeBlock, newBlock);

	log_block((tmpNodeBlock.flags & BI_OURS ? "Good +" : "Good  "), tmpNodeBlock.hash, tmpNodeBlock.time, tmpNodeBlock.storage_pos);

	i = MAIN_TIME(nodeBlock->time) & (HASHRATE_LAST_MAX_TIME - 1);
	if(MAIN_TIME(nodeBlock->time) > MAIN_TIME(g_dag_extstats.hashrate_last_time)) {
		memset(g_dag_extstats.hashrate_total + i, 0, sizeof(dag_diff_t));
		memset(g_dag_extstats.hashrate_ours + i, 0, sizeof(dag_diff_t));
		g_dag_extstats.hashrate_last_time = nodeBlock->time;
	}

	if(dag_diff_gt(diff0, g_dag_extstats.hashrate_total[i])) {
		g_dag_extstats.hashrate_total[i] = diff0;
	}

	if(tmpNodeBlock.flags & BI_OURS && dag_diff_gt(diff0, g_dag_extstats.hashrate_ours[i])) {
		g_dag_extstats.hashrate_ours[i] = diff0;
	}

end:
	for(j = 0; j < keysCount; ++j) {
		dag_free_key(public_keys[j].key);
	}

	if(err > 0) {
		char buf[32] = {0};
		err |= i << 4;
		sprintf(buf, "Err %2x", err & 0xff);
		log_block(buf, tmpNodeBlock.hash, tmpNodeBlock.time, transportHeader);
	}

	return -err;
}

void *add_block_callback(void *block, void *data)
{
	struct dag_block *b = (struct dag_block *)block;
	xtime_t *t = (xtime_t*)data;
	int res;

	pthread_mutex_lock(&block_mutex);

	if(*t < g_dag_era) {
		(res = add_block_nolock(b, *t));
	} else if((res = add_block_nolock(b, 0)) >= 0 && b->field[0].time > *t) {
		*t = b->field[0].time;
	}

	pthread_mutex_unlock(&block_mutex);

	if(res >= 0) {
		dag_sync_pop_block(b);
	}

	return 0;
}

/* 检查并向存储添加块。 出错时返回非零值。 */
int dag_add_block(struct dag_block *b)
{
	pthread_mutex_lock(&block_mutex);
	int res = add_block_nolock(b, g_time_limit);
	pthread_mutex_unlock(&block_mutex);

	return res;
}

#define setfld(fldtype, src, hashtype) ( \
		block[0].field[0].type |= (uint64_t)(fldtype) << (i << 2), \
			memcpy(&block[0].field[i++], (void*)(src), sizeof(hashtype)) \
		)

#define pretop_block() (top_main_chain && MAIN_TIME(top_main_chain->time) == MAIN_TIME(send_time) ? pretop_main_chain : top_main_chain)

/* create a new block
 * The first 'ninput' field 'fields' contains the addresses of the inputs and the corresponding quantity of XDAG,
 * in the following 'noutput' fields similarly - outputs, fee; send_time (time of sending the block);
 * if it is greater than the current one, then the mining is performed to generate the most optimal hash
 */
struct dag_block* dag_create_block(struct dag_field *fields, int inputsCount, int outputsCount, int hasRemark,
	dag_amount_t fee, xtime_t send_time, dag_hash_t block_hash_result)
{
	pthread_mutex_lock(&g_create_block_mutex);
	struct dag_block block[2];
	int i, j, res, mining, defkeynum, keysnum[DAG_BLOCK_FIELDS], nkeys, nkeysnum = 0, outsigkeyind = -1, has_pool_tag = 0;
	struct dag_public_key *defkey = dag_wallet_default_key(&defkeynum), *keys = dag_wallet_our_keys(&nkeys), *key;
	dag_hash_t signatureHash;
	dag_hash_t newBlockHash;
	struct block_internal *ref, *pretop = pretop_block();
	struct orphan_block *oref;

	for (i = 0; i < inputsCount; ++i) {
		ref = block_by_hash(fields[i].hash);
		if (!ref || !(ref->flags & BI_OURS)) {
			pthread_mutex_unlock(&g_create_block_mutex);
			return NULL;
		}

		for (j = 0; j < nkeysnum && ref->n_our_key != keysnum[j]; ++j);

		if (j == nkeysnum) {
			if (outsigkeyind < 0 && ref->n_our_key == defkeynum) {
				outsigkeyind = nkeysnum;
			}
			keysnum[nkeysnum++] = ref->n_our_key;
		}
	}
	pthread_mutex_unlock(&g_create_block_mutex);

	int res0 = 1 + inputsCount + outputsCount + hasRemark + 3 * nkeysnum + (outsigkeyind < 0 ? 2 : 0);

	if (res0 > DAG_BLOCK_FIELDS) {
		dag_err("create block failed, exceed max number of fields.");
		return NULL;
	}

	if (!send_time) {
		send_time = dag_get_xtimestamp();
		mining = 0;
	} else {
		mining = (send_time > dag_get_xtimestamp() && res0 + 1 <= DAG_BLOCK_FIELDS);
	}

	res0 += mining;

#if REMARK_ENABLED
	/* reserve field for pool tag in generated main block */
	has_pool_tag = g_pool_has_tag;
	res0 += has_pool_tag * mining;
#endif

 begin:
	res = res0;
	memset(block, 0, sizeof(struct dag_block));
	i = 1;
	block[0].field[0].type = g_block_header_type | (mining ? (uint64_t)DAG_FIELD_SIGN_IN << ((DAG_BLOCK_FIELDS - 1) * 4) : 0);
	block[0].field[0].time = send_time;
	block[0].field[0].amount = fee;

	if (g_light_mode) {
		pthread_mutex_lock(&g_create_block_mutex);
		if (res < DAG_BLOCK_FIELDS && ourfirst) {
			setfld(DAG_FIELD_OUT, ourfirst->hash, dag_hashlow_t);
			res++;
		}
		pthread_mutex_unlock(&g_create_block_mutex);
	} else {
		pthread_mutex_lock(&block_mutex);
		if (res < DAG_BLOCK_FIELDS && mining && pretop && pretop->time < send_time) {
			log_block("Mintop", pretop->hash, pretop->time, pretop->storage_pos);
			setfld(DAG_FIELD_OUT, pretop->hash, dag_hashlow_t);
			res++;
		}
		for (oref = g_orphan_first[0]; oref && res < DAG_BLOCK_FIELDS; oref = oref->next) {
			ref = oref->orphan_bi;
			if (ref->time < send_time) {
				setfld(DAG_FIELD_OUT, ref->hash, dag_hashlow_t);
				res++;
			}
		}
		pthread_mutex_unlock(&block_mutex);
	}

	for (j = 0; j < inputsCount; ++j) {
		setfld(DAG_FIELD_IN, fields + j, dag_hash_t);
	}

	for (j = 0; j < outputsCount; ++j) {
		setfld(DAG_FIELD_OUT, fields + inputsCount + j, dag_hash_t);
	}

	if(hasRemark) {
		setfld(DAG_FIELD_REMARK, fields + inputsCount + outputsCount, dag_remark_t);
	}

	if(mining && has_pool_tag) {
		setfld(DAG_FIELD_REMARK, g_pool_tag, dag_remark_t);
	}

	for (j = 0; j < nkeysnum; ++j) {
		key = keys + keysnum[j];
		block[0].field[0].type |= (uint64_t)((j == outsigkeyind ? DAG_FIELD_SIGN_OUT : DAG_FIELD_SIGN_IN) * 0x11) << ((i + j + nkeysnum) * 4);
		setfld(DAG_FIELD_PUBLIC_KEY_0 + ((uintptr_t)key->pub & 1), (uintptr_t)key->pub & ~1l, dag_hash_t);
	}

	if(outsigkeyind < 0) {
		block[0].field[0].type |= (uint64_t)(DAG_FIELD_SIGN_OUT * 0x11) << ((i + j + nkeysnum) * 4);
	}

	for (j = 0; j < nkeysnum; ++j, i += 2) {
		key = keys + keysnum[j];
		hash_for_signature(block, key, signatureHash);
		dag_sign(key->key, signatureHash, block[0].field[i].data, block[0].field[i + 1].data);
	}

	if (outsigkeyind < 0) {
		hash_for_signature(block, defkey, signatureHash);
		dag_sign(defkey->key, signatureHash, block[0].field[i].data, block[0].field[i + 1].data);
	}

	if (mining) {
		if(!do_mining(block, &pretop, send_time)) {
			goto begin;
		}
	}

	dag_hash(block, sizeof(struct dag_block), newBlockHash);

	if(mining) {
		memcpy(g_xdag_mined_hashes[MAIN_TIME(send_time) & (CONFIRMATIONS_COUNT - 1)],
			newBlockHash, sizeof(dag_hash_t));
		memcpy(g_xdag_mined_nonce[MAIN_TIME(send_time) & (CONFIRMATIONS_COUNT - 1)],
			block[0].field[DAG_BLOCK_FIELDS - 1].data, sizeof(dag_hash_t));
	}

	log_block("Create", newBlockHash, block[0].field[0].time, 1);
	
	if(block_hash_result != NULL) {
		memcpy(block_hash_result, newBlockHash, sizeof(dag_hash_t));
	}

	struct dag_block *new_block = (struct dag_block *)malloc(sizeof(struct dag_block));
	if(new_block) {
		memcpy(new_block, block, sizeof(struct dag_block));
	}	
	return new_block;
}

/* 创建并发布一个块
* 第一个“ninput”字段“fields”包含输入的地址和XDAG的相应数量,
* 在以下'noutput'字段中类似 - 输出，费用; send_time（发送块的时间）;
* 如果它大于当前值，则执行挖掘以生成最佳散列
*/
int dag_create_and_send_block(struct dag_field *fields, int inputsCount, int outputsCount, int hasRemark,
	dag_amount_t fee, xtime_t send_time, dag_hash_t block_hash_result)
{
	struct dag_block *block = dag_create_block(fields, inputsCount, outputsCount, hasRemark, fee, send_time, block_hash_result);
	if(!block) {
		return 0;
	}

	block->field[0].transport_header = 1;
	int res = dag_add_block(block);
	if(res > 0) {
		dag_send_new_block(block);
		res = 1;
	} else {
		res = 0;
	}
	free(block);

	return res;
}

int do_mining(struct dag_block *block, struct block_internal **pretop, xtime_t send_time)
{
	uint64_t taskIndex = g_dag_pool_task_index + 1;
	struct dag_pool_task *task = &g_dag_pool_task[taskIndex & 1];

	dag_generate_random_array(block[0].field[DAG_BLOCK_FIELDS - 1].data, sizeof(dag_hash_t));

	task->task_time = MAIN_TIME(send_time);

	dag_hash_init(task->ctx0);
	dag_hash_update(task->ctx0, block, sizeof(struct dag_block) - 2 * sizeof(struct dag_field));
	dag_hash_get_state(task->ctx0, task->task[0].data);
	dag_hash_update(task->ctx0, block[0].field[DAG_BLOCK_FIELDS - 2].data, sizeof(struct dag_field));
	memcpy(task->ctx, task->ctx0, dag_hash_ctx_size());

	dag_hash_update(task->ctx, block[0].field[DAG_BLOCK_FIELDS - 1].data, sizeof(struct dag_field) - sizeof(uint64_t));
	memcpy(task->task[1].data, block[0].field[DAG_BLOCK_FIELDS - 2].data, sizeof(struct dag_field));
	memcpy(task->nonce.data, block[0].field[DAG_BLOCK_FIELDS - 1].data, sizeof(struct dag_field));
	memcpy(task->lastfield.data, block[0].field[DAG_BLOCK_FIELDS - 1].data, sizeof(struct dag_field));

	dag_hash_final(task->ctx, &task->nonce.amount, sizeof(uint64_t), task->minhash.data);
	g_dag_pool_task_index = taskIndex;

	while(dag_get_xtimestamp() <= send_time) {
		sleep(1);
		pthread_mutex_lock(&g_create_block_mutex);
		struct block_internal *pretop_new = pretop_block();
		pthread_mutex_unlock(&g_create_block_mutex);
		if(*pretop != pretop_new && dag_get_xtimestamp() < send_time) {
			*pretop = pretop_new;
			dag_info("Mining: start from beginning because of pre-top block changed");
			return 0;
		}
	}

	pthread_mutex_lock((pthread_mutex_t*)g_ptr_share_mutex);
	memcpy(block[0].field[DAG_BLOCK_FIELDS - 1].data, task->lastfield.data, sizeof(struct dag_field));
	pthread_mutex_unlock((pthread_mutex_t*)g_ptr_share_mutex);

	return 1;
}

static void reset_callback(struct ldus_rbtree *node)
{
	struct block_internal *bi = 0;

	if(g_bi_index_enable) {
		struct block_internal_index *index = (struct block_internal_index *)node;
		bi = index->bi;
	} else {
		bi = (struct block_internal *)node;
	}

	struct block_backrefs *tmp;
	for(struct block_backrefs *to_free = (struct block_backrefs*)atomic_load_explicit_uintptr(&bi->backrefs, memory_order_acquire); to_free != NULL;){
		tmp = to_free->next;
		dag_free(to_free);
		to_free = tmp;
	}
	if((bi->flags & BI_REMARK) && bi->remark != (uintptr_t)NULL) {
		dag_free((char*)bi->remark);
	}
	dag_free(bi);

	if(g_bi_index_enable) {
		free(node);
	}
}

// main thread which works with block
static void *work_thread(void *arg)
{
	xtime_t t = g_dag_era, conn_time = 0, sync_time = 0, t0;
	int n_mining_threads = (int)(unsigned)(uintptr_t)arg, sync_thread_running = 0;
	uint64_t nhashes0 = 0, nhashes = 0;
	pthread_t th;
	uint64_t last_nmain = 0, nmain;
	time_t last_time_nmain_unequal = time(NULL);

begin:
	// loading block from the local storage
	g_dag_state = DAG_STATE_LOAD;
	dag_mess("Loading blocks from local storage...");

	xtime_t start = dag_get_xtimestamp();
	dag_show_state(0);

	dag_load_blocks(t, dag_get_xtimestamp(), &t, &add_block_callback);

	dag_mess("Finish loading blocks, time cost %ldms", dag_get_xtimestamp() - start);

	// waiting for command "run"
	while (!g_dag_run) {
		g_dag_state = DAG_STATE_STOP;
		sleep(1);
	}

	// launching of synchronization thread
	g_dag_sync_on = 1;
	if (!g_light_mode && !sync_thread_running) {
		dag_mess("Starting sync thread...");
		int err = pthread_create(&th, 0, sync_thread, 0);
		if(err != 0) {
			printf("create sync_thread failed, error : %s\n", strerror(err));
			return 0;
		}

		sync_thread_running = 1;

		err = pthread_detach(th);
		if(err != 0) {
			printf("detach sync_thread failed, error : %s\n", strerror(err));
			return 0;
		}
	}

	if (g_light_mode) {
		// start mining threads
		dag_mess("Starting mining threads...");
		dag_mining_start(n_mining_threads);
	}

	// periodic generation of blocks and determination of the main block
	dag_mess("Entering main cycle...");

	for (;;) {
		unsigned nblk;

		t0 = t;
		t = dag_get_xtimestamp();
		nhashes0 = nhashes;
		nhashes = g_dag_extstats.nhashes;
		nmain = g_dag_stats.nmain;

		if (t > t0) {
			g_dag_extstats.hashrate_s = ((double)(nhashes - nhashes0) * 1024) / (t - t0);
		}

		if (!g_block_production_on && !g_light_mode &&
				(g_dag_state == DAG_STATE_WAIT || g_dag_state == DAG_STATE_WTST ||
				g_dag_state == DAG_STATE_SYNC || g_dag_state == DAG_STATE_STST || 
				g_dag_state == DAG_STATE_CONN || g_dag_state == DAG_STATE_CTST)) {
			if (g_dag_state == DAG_STATE_SYNC || g_dag_state == DAG_STATE_STST || 
					g_dag_stats.nmain >= (MAIN_TIME(t) - dag_start_main_time())) {
				g_block_production_on = 1;
			} else if (last_nmain != nmain) {
				last_nmain = nmain;
				last_time_nmain_unequal = time(NULL);
			} else if (time(NULL) - last_time_nmain_unequal > MAX_TIME_NMAIN_STALLED) {
				g_block_production_on = 1;
			}

			if (g_block_production_on) {
				dag_mess("Starting refer blocks creation...");

				// start mining threads
				dag_mess("Starting mining threads...");
				dag_mining_start(n_mining_threads);
			}

		}

		if (g_block_production_on && 
				(nblk = (unsigned)g_dag_extstats.nnoref / (DAG_BLOCK_FIELDS - 5))) {
			nblk = nblk / 61 + (nblk % 61 > (unsigned)rand() % 61);

			while (nblk--) {
				dag_create_and_send_block(0, 0, 0, 0, 0, 0, NULL);
			}
		}

		pthread_mutex_lock(&block_mutex);

		if (g_dag_state == DAG_STATE_REST) {
			g_dag_sync_on = 0;
			pthread_mutex_unlock(&block_mutex);
			dag_mining_start(0);

			while (dag_get_xtimestamp() - t < MAIN_CHAIN_PERIOD + (3 << 10)) {
				sleep(1);
			}

			pthread_mutex_lock(&block_mutex);

			if (dag_free_all()) {
				pthread_mutex_lock(&rbtree_mutex);
				ldus_rbtree_walk_up(root, reset_callback);
				pthread_mutex_unlock(&rbtree_mutex);
			}
			
			root = 0;
			g_balance = 0;
			top_main_chain = pretop_main_chain = 0;
			ourfirst = ourlast = 0;
			g_orphan_first[0] = g_orphan_last[0] = 0;
			g_orphan_first[1] = g_orphan_last[1] = 0;
			memset(&g_dag_stats, 0, sizeof(g_dag_stats));
			memset(&g_dag_extstats, 0, sizeof(g_dag_extstats));
			pthread_mutex_unlock(&block_mutex);
			conn_time = sync_time = 0;

			goto begin;
		} else {
			pthread_mutex_lock(&g_transport_mutex);
			if (t > (g_dag_last_received << 10) && t - (g_dag_last_received << 10) > 3 * MAIN_CHAIN_PERIOD) {
				g_dag_state = (g_light_mode ? (g_dag_testnet ? DAG_STATE_TTST : DAG_STATE_TRYP)
					: (g_dag_testnet ? DAG_STATE_WTST : DAG_STATE_WAIT));
				conn_time = sync_time = 0;
			} else {
				if (!conn_time) {
					conn_time = t;
				}

				if (!g_light_mode && t - conn_time >= 2 * MAIN_CHAIN_PERIOD
					&& !memcmp(&g_dag_stats.difficulty, &g_dag_stats.max_difficulty, sizeof(dag_diff_t))) {
					sync_time = t;
				}

				if (t - (g_dag_xfer_last << 10) <= 2 * MAIN_CHAIN_PERIOD + 4) {
					g_dag_state = DAG_STATE_XFER;
				} else if (g_light_mode) {
					g_dag_state = (g_dag_mining_threads > 0 ?
						(g_dag_testnet ? DAG_STATE_MTST : DAG_STATE_MINE)
						: (g_dag_testnet ? DAG_STATE_PTST : DAG_STATE_POOL));
				} else if (t - sync_time > 8 * MAIN_CHAIN_PERIOD) {
					g_dag_state = (g_dag_testnet ? DAG_STATE_CTST : DAG_STATE_CONN);
				} else {
					g_dag_state = (g_dag_testnet ? DAG_STATE_STST : DAG_STATE_SYNC);
				}
			}
			pthread_mutex_unlock(&g_transport_mutex);
		}

		if (!g_light_mode) {
			check_new_main();
		}

		struct block_internal *ours = ourfirst;
		pthread_mutex_unlock(&block_mutex);
		dag_show_state(ours ? ours->hash : 0);

		while (dag_get_xtimestamp() - t < 1024) {
			sleep(1);
		}
	}

	return 0;
}

/* 开始常规块处理
 * n_mining_threads - the number of threads for mining on the CPU;
 *   for the light node is_pool == 0;
 * miner_address = 1 - the address of the miner is explicitly set
 */
int dag_blocks_start(int is_pool, int mining_threads_count, int miner_address)
{
	pthread_mutexattr_t attr;
	pthread_t th;

	if (!is_pool) {
		g_light_mode = 1;
	}

	if (dag_mem_init(g_light_mode && !miner_address ? 0 : (((dag_get_xtimestamp() - g_dag_era) >> 10) + (uint64_t)365 * 24 * 60 * 60) * 2 * sizeof(struct block_internal))) {
		return -1;
	}

	g_bi_index_enable = g_use_tmpfile;

	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&block_mutex, &attr);
	pthread_mutex_init(&rbtree_mutex, 0);
	int err = pthread_create(&th, 0, work_thread, (void*)(uintptr_t)(unsigned)mining_threads_count);
	if(err != 0) {
		printf("create work_thread failed, error : %s\n", strerror(err));
		return -1;
	}
	err = pthread_detach(th);
	if(err != 0) {
		printf("create pool_main_thread failed, error : %s\n", strerror(err));
		return -1;
	}

	return 0;
}

/* 返回我们的第一个块。 如果还没有块 - 则创建第一个块。 */
int dag_get_our_block(dag_hash_t hash)
{
	pthread_mutex_lock(&block_mutex);
	struct block_internal *bi = ourfirst;
	pthread_mutex_unlock(&block_mutex);

	if (!bi) {
		dag_create_and_send_block(0, 0, 0, 0, 0, 0, NULL);
		pthread_mutex_lock(&block_mutex);
		bi = ourfirst;
		pthread_mutex_unlock(&block_mutex);
		if (!bi) {
			return -1;
		}
	}

	memcpy(hash, bi->hash, sizeof(dag_hash_t));

	return 0;
}

/* 为每个自己的块调用回调 */
int dag_traverse_our_blocks(void *data,
    int (*callback)(void*, dag_hash_t, dag_amount_t, xtime_t, int))
{
	int res = 0;

	pthread_mutex_lock(&block_mutex);

	for (struct block_internal *bi = ourfirst; !res && bi; bi = bi->ournext) {
		res = (*callback)(data, bi->hash, bi->amount, bi->time, bi->n_our_key);
	}

	pthread_mutex_unlock(&block_mutex);

	return res;
}

static int (*g_traverse_callback)(void *data, dag_hash_t hash, dag_amount_t amount, xtime_t time);
static void *g_traverse_data;

static void traverse_all_callback(struct ldus_rbtree *node)
{
	struct block_internal *bi = 0;
	if(g_bi_index_enable) {
		struct block_internal_index *index = (struct block_internal_index *)node;
		bi = index->bi;
	} else {
		bi = (struct block_internal *)node;
	}

	(*g_traverse_callback)(g_traverse_data, bi->hash, bi->amount, bi->time);
}

/* 调用每个块的回调 */
int dag_traverse_all_blocks(void *data, int (*callback)(void *data, dag_hash_t hash,
	dag_amount_t amount, xtime_t time))
{
	pthread_mutex_lock(&block_mutex);
	g_traverse_callback = callback;
	g_traverse_data = data;
	pthread_mutex_lock(&rbtree_mutex);
	ldus_rbtree_walk_right(root, traverse_all_callback);
	pthread_mutex_unlock(&rbtree_mutex);
	pthread_mutex_unlock(&block_mutex);
	return 0;
}

/* 返回指定地址的当前余额，如果hash==0，则返回所有地址的余额 */
dag_amount_t dag_get_balance(dag_hash_t hash)
{
	if (!hash) {
		return g_balance;
	}

	struct block_internal *bi = block_by_hash(hash);

	if (!bi) {
		return 0;
	}

	return bi->amount;
}

/* 为指定地址设置当前余额 */
int dag_set_balance(dag_hash_t hash, dag_amount_t balance)
{
	if (!hash) return -1;

	pthread_mutex_lock(&block_mutex);
	struct block_internal *bi = block_by_hash(hash);
	if (bi->flags & BI_OURS && bi != ourfirst) {
		if (bi->ourprev) {
			bi->ourprev->ournext = bi->ournext;
		} else {
			ourfirst = bi->ournext;
		}

		if (bi->ournext) {
			bi->ournext->ourprev = bi->ourprev;
		} else {
			ourlast = bi->ourprev;
		}

		bi->ourprev = 0;
		bi->ournext = ourfirst;

		if (ourfirst) {
			ourfirst->ourprev = bi;
		} else {
			ourlast = bi;
		}

		ourfirst = bi;
	}

	pthread_mutex_unlock(&block_mutex);

	if (!bi) return -1;

	if (bi->amount != balance) {
		dag_hash_t hash0;
		dag_amount_t diff;

		memset(hash0, 0, sizeof(dag_hash_t));

		if (balance > bi->amount) {
			diff = balance - bi->amount;
			dag_log_xfer(hash0, hash, diff);
			if (bi->flags & BI_OURS) {
				g_balance += diff;
			}
		} else {
			diff = bi->amount - balance;
			dag_log_xfer(hash, hash0, diff);
			if (bi->flags & BI_OURS) {
				g_balance -= diff;
			}
		}

		bi->amount = balance;
	}

	return 0;
}

// 通过散列返回块的位置和时间；如果块是额外的和块！= 0还返回整个块
int64_t dag_get_block_pos(const dag_hash_t hash, xtime_t *t, struct dag_block *block)
{
	if (block) pthread_mutex_lock(&block_mutex);
	struct block_internal *bi = block_by_hash(hash);

	if (!bi) {
		if (block) pthread_mutex_unlock(&block_mutex);
		return -1;
	}

	if (block && bi->flags & BI_EXTRA) {
		memcpy(block, bi->oref->block, sizeof(struct dag_block));
	}

	if (block) pthread_mutex_unlock(&block_mutex);

	*t = bi->time;

	return bi->storage_pos;
}

//通过块的散列返回键的数目，如果块不是我们的，则返回-1。
int dag_get_key(dag_hash_t hash)
{
	struct block_internal *bi = block_by_hash(hash);

	if (!bi || !(bi->flags & BI_OURS)) {
		return -1;
	}

	return bi->n_our_key;
}

/*块处理的重新初始化*/
int dag_blocks_reset(void)
{
	pthread_mutex_lock(&block_mutex);
	if (g_dag_state != DAG_STATE_REST) {
		dag_crit("The local storage is corrupted. Resetting blocks engine.");
		g_dag_state = DAG_STATE_REST;
		dag_show_state(0);
	}
	pthread_mutex_unlock(&block_mutex);

	return 0;
}

#define pramount(amount) xdag_amount2xdag(amount), xdag_amount2cheato(amount)

static int bi_compar(const void *l, const void *r)
{
	xtime_t tl = (*(struct block_internal **)l)->time, tr = (*(struct block_internal **)r)->time;

	return (tl < tr) - (tl > tr);
}

// 返回块状态的字符串表示形式。不顾两面旗帜. 不兼容BI_OURS版本
const char* dag_get_block_state_info(uint8_t flags)
{
	const uint8_t flag = flags & ~(BI_OURS | BI_REMARK);

	if(flag == (BI_REF | BI_MAIN_REF | BI_APPLIED | BI_MAIN | BI_MAIN_CHAIN)) { //1F
		return "Main";
	}
	if(flag == (BI_REF | BI_MAIN_REF | BI_APPLIED)) { //1C
		return "Accepted";
	}
	if(flag == (BI_REF | BI_MAIN_REF)) { //18
		return "Rejected";
	}
	return "Pending";
}

/* 打印有关块的详细信息 */
int dag_print_block_info(dag_hash_t hash, FILE *out)
{
	char time_buf[64] = {0};
	char address[33] = {0};
	int i;

	struct block_internal *bi = block_by_hash(hash);

	if (!bi) {
		return -1;
	}

	uint64_t *h = bi->hash;
	dag_xtime_to_string(bi->time, time_buf);
	fprintf(out, "      time: %s\n", time_buf);
	fprintf(out, " timestamp: %llx\n", (unsigned long long)bi->time);
	fprintf(out, "     flags: %x\n", bi->flags & ~BI_OURS);
	fprintf(out, "     state: %s\n", dag_get_block_state_info(bi->flags));
	fprintf(out, "  file pos: %llx\n", (unsigned long long)bi->storage_pos);
	fprintf(out, "      hash: %016llx%016llx%016llx%016llx\n",
		(unsigned long long)h[3], (unsigned long long)h[2], (unsigned long long)h[1], (unsigned long long)h[0]);
	fprintf(out, "    remark: %s\n", get_remark(bi));
	fprintf(out, "difficulty: %llx%016llx\n", xdag_diff_args(bi->difficulty));
	dag_hash2address(h, address);
	fprintf(out, "   balance: %s  %10u.%09u\n", address, pramount(bi->amount));
	fprintf(out, "-----------------------------------------------------------------------------------------------------------------------------\n");
	fprintf(out, "                               block as transaction: details\n");
	fprintf(out, " direction  address                                    amount\n");
	fprintf(out, "-----------------------------------------------------------------------------------------------------------------------------\n");
	int flags;
	struct block_internal *ref;
	pthread_mutex_lock(&block_mutex);
	ref = bi->ref;
	flags = bi->flags;
	pthread_mutex_unlock(&block_mutex);
	if((flags & BI_REF) && ref != NULL) {
		dag_hash2address(ref->hash, address);
	} else {
		strcpy(address, "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
	}
	fprintf(out, "       fee: %s  %10u.%09u\n", address, pramount(bi->fee));

 	if(flags & BI_EXTRA) pthread_mutex_lock(&block_mutex);
 	int nlinks = bi->nlinks;
	struct block_internal *link[MAX_LINKS];
	memcpy(link, bi->link, nlinks * sizeof(struct block_internal*));
	if(flags & BI_EXTRA) pthread_mutex_unlock(&block_mutex);

 	for (i = 0; i < nlinks; ++i) {
		dag_hash2address(link[i]->hash, address);
		fprintf(out, "    %6s: %s  %10u.%09u\n", (1 << i & bi->in_mask ? " input" : "output"),
			address, pramount(bi->linkamount[i]));
	}

	fprintf(out, "-----------------------------------------------------------------------------------------------------------------------------\n");
	fprintf(out, "                                 block as address: details\n");
	fprintf(out, " direction  transaction                                amount       time                     remark                          \n");
	fprintf(out, "-----------------------------------------------------------------------------------------------------------------------------\n");

	int N = 0x10000;
	int n = 0;
	struct block_internal **ba = malloc(N * sizeof(struct block_internal *));

	if (!ba) return -1;

	for (struct block_backrefs *br = (struct block_backrefs*)atomic_load_explicit_uintptr(&bi->backrefs, memory_order_acquire); br; br = br->next) {
		for (i = N_BACKREFS; i && !br->backrefs[i - 1]; i--);

		if (!i) {
			continue;
		}

		if (n + i > N) {
			N *= 2;
			struct block_internal **ba1 = realloc(ba, N * sizeof(struct block_internal *));
			if (!ba1) {
				free(ba);
				return -1;
			}

			ba = ba1;
		}

		memcpy(ba + n, br->backrefs, i * sizeof(struct block_internal *));
		n += i;
	}

	if (n) {
		qsort(ba, n, sizeof(struct block_internal *), bi_compar);

		for (i = 0; i < n; ++i) {
			if (!i || ba[i] != ba[i - 1]) {
				struct block_internal *ri = ba[i];
				if (ri->flags & BI_APPLIED) {
					for (int j = 0; j < ri->nlinks; j++) {
						if(ri->link[j] == bi && ri->linkamount[j]) {
							dag_xtime_to_string(ri->time, time_buf);
							dag_hash2address(ri->hash, address);
							fprintf(out, "    %6s: %s  %10u.%09u  %s  %s\n",
								(1 << j & ri->in_mask ? "output" : " input"), address,
								pramount(ri->linkamount[j]), time_buf, get_remark(ri));
						}
					}
				}
			}
		}
	}

	free(ba);
	
	if (bi->flags & BI_MAIN) {
		dag_hash2address(h, address);
		fprintf(out, "   earning: %s  %10u.%09u  %s\n", address,
			pramount(MAIN_START_AMOUNT >> ((MAIN_TIME(bi->time) - MAIN_TIME(g_dag_era)) >> MAIN_BIG_PERIOD_LOG)),
			time_buf);
	}
	
	return 0;
}

static inline void print_block(struct block_internal *block, int print_only_addresses, FILE *out)
{
	char address[33] = {0};
	char time_buf[64] = {0};

	dag_hash2address(block->hash, address);

	if(print_only_addresses) {
		fprintf(out, "%s\n", address);
	} else {
		dag_xtime_to_string(block->time, time_buf);
		fprintf(out, "%s   %s   %-8s  %-32s\n", address, time_buf, dag_get_block_state_info(block->flags), get_remark(block));
	}
}

static inline void print_header_block_list(FILE *out)
{
	fprintf(out, "---------------------------------------------------------------------------------------------------------\n");
	fprintf(out, "address                            time                      state     mined by                          \n");
	fprintf(out, "---------------------------------------------------------------------------------------------------------\n");
}

//最后n个主要块的打印列表
void dag_list_main_blocks(int count, int print_only_addresses, FILE *out)
{
	int i = 0;
	if(!print_only_addresses) {
		print_header_block_list(out);
	}

	pthread_mutex_lock(&block_mutex);

	for (struct block_internal *b = top_main_chain; b && i < count; b = b->link[b->max_diff_link]) {
		if (b->flags & BI_MAIN) {
			print_block(b, print_only_addresses, out);
			++i;
		}
	}

	pthread_mutex_unlock(&block_mutex);
}

// 由当前池挖掘的N个最后块的打印列表
// TODO:找到找到找到非付费挖掘块或删除“include_non_payed”参数的方法
void dag_list_mined_blocks(int count, int include_non_payed, FILE *out)
{
	int i = 0;
	print_header_block_list(out);

	pthread_mutex_lock(&block_mutex);

	for(struct block_internal *b = top_main_chain; b && i < count; b = b->link[b->max_diff_link]) {
		if(b->flags & BI_MAIN && b->flags & BI_OURS) {
			print_block(b, 0, out);
			++i;
		}
	}

	pthread_mutex_unlock(&block_mutex);
}

void cache_retarget(int32_t cache_hit, int32_t cache_miss)
{
	if(g_dag_extstats.cache_usage >= g_dag_extstats.cache_size) {
		if(g_dag_extstats.cache_hitrate < 0.94 && g_dag_extstats.cache_size < CACHE_MAX_SIZE) {
			g_dag_extstats.cache_size++;
		} else if(g_dag_extstats.cache_hitrate > 0.98 && !cache_miss && g_dag_extstats.cache_size && (rand() & 0xF) < 0x5) {
			g_dag_extstats.cache_size--;
		}
		for(int l = g_dag_extstats.cache_usage; l > g_dag_extstats.cache_size; l--) {
			if(cache_first != NULL) {
				struct cache_block* to_free = cache_first;
				cache_first = cache_first->next;
				if(cache_first == NULL) {
					cache_last = NULL;
				}
				ldus_rbtree_remove(&cache_root, &to_free->node);
				free(to_free);
				g_dag_extstats.cache_usage--;
			} else {
				break;
				dag_warn("Non critical error, break in for [function: cache_retarget]");
			}
		}

	} else if(g_dag_extstats.cache_hitrate > 0.98 && !cache_miss && g_dag_extstats.cache_size && (rand() & 0xF) < 0x5) {
		g_dag_extstats.cache_size--;
	}
	if((uint32_t)(g_dag_extstats.cache_size / 0.9) > CACHE_MAX_SIZE) {
		g_dag_extstats.cache_size = (uint32_t)(g_dag_extstats.cache_size*0.9);
	}
	if(cache_hit + cache_miss > 0) {
		if(cache_bounded_counter < CACHE_MAX_SAMPLES)
			cache_bounded_counter++;
		g_dag_extstats.cache_hitrate = moving_average_double(g_dag_extstats.cache_hitrate, (double)((cache_hit) / (cache_hit + cache_miss)), cache_bounded_counter);

	}
}

void cache_add(struct dag_block* block, dag_hash_t hash)
{
	if(g_dag_extstats.cache_usage <= CACHE_MAX_SIZE) {
		struct cache_block *cacheBlock = malloc(sizeof(struct cache_block));
		if(cacheBlock != NULL) {
			memset(cacheBlock, 0, sizeof(struct cache_block));
			memcpy(&(cacheBlock->block), block, sizeof(struct dag_block));
			memcpy(&(cacheBlock->hash), hash, sizeof(dag_hash_t));

			if(cache_first == NULL)
				cache_first = cacheBlock;
			if(cache_last != NULL)
				cache_last->next = cacheBlock;
			cache_last = cacheBlock;
			ldus_rbtree_insert(&cache_root, &cacheBlock->node);
			g_dag_extstats.cache_usage++;
		} else {
			dag_warn("cache malloc failed [function: cache_add]");
		}
	} else {
		dag_warn("maximum cache reached [function: cache_add]");
	}

}

int32_t check_signature_out_cached(struct block_internal* blockRef, struct dag_public_key *public_keys, const int keysCount, int32_t *cache_hit, int32_t *cache_miss)
{
	struct cache_block *bref = cache_block_by_hash(blockRef->hash);
	if(bref != NULL) {
		++(*cache_hit);
		return  find_and_verify_signature_out(&(bref->block), public_keys, keysCount);
	} else {
		++(*cache_miss);
		return check_signature_out(blockRef, public_keys, keysCount);
	}
}

int32_t check_signature_out(struct block_internal* blockRef, struct dag_public_key *public_keys, const int keysCount)
{
	struct dag_block buf;
	struct dag_block *bref = dag_storage_load(blockRef->hash, blockRef->time, blockRef->storage_pos, &buf);
	if(!bref) {
		return 8;
	}
	return find_and_verify_signature_out(bref, public_keys, keysCount);
}

static int32_t find_and_verify_signature_out(struct dag_block* bref, struct dag_public_key *public_keys, const int keysCount)
{
	int j = 0;
	for(int k = 0; j < DAG_BLOCK_FIELDS; ++j) {
		if(dag_type(bref, j) == DAG_FIELD_SIGN_OUT && (++k & 1)
			&& valid_signature(bref, j, keysCount, public_keys) >= 0) {
			break;
		}
	}
	if(j == DAG_BLOCK_FIELDS) {
		return 9;
	}
	return 0;
}

int dag_get_transactions(dag_hash_t hash, void *data, int (*callback)(void*, int, int, dag_hash_t, dag_amount_t, xtime_t, const char *))
{
	struct block_internal *bi = block_by_hash(hash);
	
	if (!bi) {
		return -1;
	}
	
	int size = 0x10000; 
	int n = 0;
	struct block_internal **block_array = malloc(size * sizeof(struct block_internal *));
	
	if (!block_array) return -1;
	
	int i;
	for (struct block_backrefs *br = (struct block_backrefs*)atomic_load_explicit_uintptr(&bi->backrefs, memory_order_acquire); br; br = br->next) {
		for (i = N_BACKREFS; i && !br->backrefs[i - 1]; i--);
		
		if (!i) {
			continue;
		}
		
		if (n + i > size) {
			size *= 2;
			struct block_internal **tmp_array = realloc(block_array, size * sizeof(struct block_internal *));
			if (!tmp_array) {
				free(block_array);
				return -1;
			}
			
			block_array = tmp_array;
		}
		
		memcpy(block_array + n, br->backrefs, i * sizeof(struct block_internal *));
		n += i;
	}
	
	if (!n) {
		free(block_array);
		return 0;
	}
	
	qsort(block_array, n, sizeof(struct block_internal *), bi_compar);
	
	for (i = 0; i < n; ++i) {
		if (!i || block_array[i] != block_array[i - 1]) {
			struct block_internal *ri = block_array[i];
			for (int j = 0; j < ri->nlinks; j++) {
				if(ri->link[j] == bi && ri->linkamount[j]) {
					if(callback(data, 1 << j & ri->in_mask, ri->flags, ri->hash, ri->linkamount[j], ri->time, get_remark(ri))) {
						free(block_array);
						return n;
					}
				}
			}
		}
	}
	
	free(block_array);
	
	return n;
}

void remove_orphan(struct block_internal* bi, int remove_action)
{
	if(!(bi->flags & BI_REF) && (remove_action != ORPHAN_REMOVE_EXTRA || (bi->flags & BI_EXTRA))) {
		struct orphan_block *obt = bi->oref;
		if (obt == NULL) {
			dag_crit("Critical error. obt=0");
		} else if (obt->orphan_bi != bi) {
			dag_crit("Critical error. bi=%p, flags=%x, action=%d, obt=%p, prev=%p, next=%p, obi=%p",
				  bi, bi->flags, remove_action, obt, obt->prev, obt->next, obt->orphan_bi);
		} else {
			int index = get_orphan_index(bi), i;
			struct orphan_block *prev = obt->prev, *next = obt->next;

			*(prev ? &(prev->next) : &g_orphan_first[index]) = next;
			*(next ? &(next->prev) : &g_orphan_last[index]) = prev;

			if (index) {
				if (remove_action != ORPHAN_REMOVE_REUSE) {
					bi->storage_pos = dag_storage_save(obt->block);
					for (i = 0; i < bi->nlinks; ++i) {
						remove_orphan(bi->link[i], ORPHAN_REMOVE_NORMAL);
					}
				}
				bi->flags &= ~BI_EXTRA;
				g_dag_extstats.nextra--;
			} else {
				g_dag_extstats.nnoref--;
			}

			bi->oref = 0;
			bi->flags |= BI_REF;
			free(obt);
		}
	}
}

void add_orphan(struct block_internal* bi, struct dag_block *block)
{
	int index = get_orphan_index(bi);
	struct orphan_block *obt = malloc(sizeof(struct orphan_block) + index * sizeof(struct dag_block));
	if(obt == NULL){
		dag_crit("Error. Malloc failed. [function: add_orphan]");
	} else {
		obt->orphan_bi = bi;
		obt->prev = g_orphan_last[index];
		obt->next = 0;
		bi->oref = obt;
		*(g_orphan_last[index] ? &g_orphan_last[index]->next : &g_orphan_first[index]) = obt;
		g_orphan_last[index] = obt;
		if (index) {
			memcpy(obt->block, block, sizeof(struct dag_block));
			g_dag_extstats.nextra++;
		} else {
			g_dag_extstats.nnoref++;
		}
	}
}

void dag_list_orphan_blocks(int count, FILE *out)
{
	int i = 0;
	print_header_block_list(out);

	pthread_mutex_lock(&block_mutex);

	for(struct orphan_block *b = g_orphan_first[0]; b && i < count; b = b->next, i++) {
		print_block(b->orphan_bi, 0, out);
	}

	pthread_mutex_unlock(&block_mutex);
}

// 用块完成工作
void dag_block_finish()
{
	pthread_mutex_lock(&g_create_block_mutex);
	pthread_mutex_lock(&block_mutex);
}

int dag_get_block_info(dag_hash_t hash, void *info, int (*info_callback)(void*, int, dag_hash_t, dag_amount_t, xtime_t, const char *),
						void *links, int (*links_callback)(void*, const char *, dag_hash_t, dag_amount_t))
{
	pthread_mutex_lock(&block_mutex);
	struct block_internal *bi = block_by_hash(hash);
	pthread_mutex_unlock(&block_mutex);

	if(info_callback && bi) {
		info_callback(info, bi->flags & ~BI_OURS,  bi->hash, bi->amount, bi->time, get_remark(bi));
	}

	if(links_callback && bi) {
		int flags;
		struct block_internal *ref;
		pthread_mutex_lock(&block_mutex);
		ref = bi->ref;
		flags = bi->flags;
		pthread_mutex_unlock(&block_mutex);

		dag_hash_t link_hash;
		memset(link_hash, 0, sizeof(dag_hash_t));
		if((flags & BI_REF) && ref != NULL) {
			memcpy(link_hash, ref->hash, sizeof(dag_hash_t));
		}
		links_callback(links, "fee", link_hash, bi->fee);

		struct block_internal *bi_links[MAX_LINKS] = {0};
		int bi_nlinks = 0;

		if(flags & BI_EXTRA) {
			pthread_mutex_lock(&block_mutex);
		}

		bi_nlinks = bi->nlinks;
		memcpy(bi_links, bi->link, bi_nlinks * sizeof(struct block_internal *));

		if(flags & BI_EXTRA) {
			pthread_mutex_unlock(&block_mutex);
		}

		for (int i = 0; i < bi_nlinks; ++i) {
			links_callback(links, (1 << i & bi->in_mask ? " input" : "output"), bi_links[i]->hash, bi->linkamount[i]);
		}
	}
	return 0;
}

static inline size_t remark_acceptance(dag_remark_t origin)
{
	char remark_buf[33] = {0};
	memcpy(remark_buf, origin, sizeof(dag_remark_t));
	size_t size = validate_remark(remark_buf);
	if(size){
		return size;
	}
	return 0;
}

static int add_remark_bi(struct block_internal* bi, dag_remark_t strbuf)
{
	size_t size = remark_acceptance(strbuf);
	char *remark_tmp = dag_malloc(size + 1);
	if(remark_tmp == NULL) {
		dag_err("xdag_malloc failed, [function add_remark_bi]");
		return 0;
	}
	memset(remark_tmp, 0, size + 1);
	memcpy(remark_tmp, strbuf, size);
	uintptr_t expected_value = 0 ;
	if(!atomic_compare_exchange_strong_explicit_uintptr(&bi->remark, &expected_value, (uintptr_t)remark_tmp, memory_order_acq_rel, memory_order_relaxed)){
		free(remark_tmp);
	}
	return 1;
}

static void add_backref(struct block_internal* blockRef, struct block_internal* nodeBlock)
{
	int i = 0;

	struct block_backrefs *tmp = (struct block_backrefs*)atomic_load_explicit_uintptr(&blockRef->backrefs, memory_order_acquire);
	// LIFO list: if the first element doesn't exist or it is full, a new element of the backrefs list will be created
	// and added as first element of backrefs block list
	if( tmp == NULL || tmp->backrefs[N_BACKREFS - 1]) {
		struct block_backrefs *blockRefs_to_insert = dag_malloc(sizeof(struct block_backrefs));
		if(blockRefs_to_insert == NULL) {
			dag_err("xdag_malloc failed. [function add_backref]");
			return;
		}
		memset(blockRefs_to_insert, 0, sizeof(struct block_backrefs));
		blockRefs_to_insert->next = tmp;
		atomic_store_explicit_uintptr(&blockRef->backrefs, (uintptr_t)blockRefs_to_insert, memory_order_release);
		tmp = blockRefs_to_insert;
	}

	// searching the first free array element
	for(; tmp->backrefs[i]; ++i);
	// adding the actual block memory address to the backrefs array
	tmp->backrefs[i] = nodeBlock;
}

static inline int get_nfield(struct dag_block *bref, int field_type)
{
	for(int i = 0; i < DAG_BLOCK_FIELDS; ++i) {
		if(dag_type(bref, i) == field_type){
			return i;
		}
	}
	return -1;
}

static inline const char* get_remark(struct block_internal *bi){
	if((bi->flags & BI_REMARK) & ~BI_EXTRA){
		const char* tmp = (const char*)atomic_load_explicit_uintptr(&bi->remark, memory_order_acquire);
		if(tmp != NULL){
			return tmp;
		} else if(load_remark(bi)){
			return (const char*)atomic_load_explicit_uintptr(&bi->remark, memory_order_relaxed);
		}
	}
	return "";
}

static int load_remark(struct block_internal* bi) {
	struct dag_block buf;
	struct dag_block *bref = dag_storage_load(bi->hash, bi->time, bi->storage_pos, &buf);
	if(bref == NULL) {
		return 0;
	}

	int remark_field = get_nfield(bref, DAG_FIELD_REMARK);
	if (remark_field < 0) {
		dag_err("Remark field not found [function: load_remark]");
		pthread_mutex_lock(&block_mutex);
		bi->flags &= ~BI_REMARK;
		pthread_mutex_unlock(&block_mutex);
		return 0;
	}
	return add_remark_bi(bi, bref->field[remark_field].remark);
}

void order_ourblocks_by_amount(struct block_internal *bi)
{
	struct block_internal *ti;
	while ((ti = bi->ourprev) && bi->amount > ti->amount) {
		bi->ourprev = ti->ourprev;
		ti->ournext = bi->ournext;
		bi->ournext = ti;
		ti->ourprev = bi;
		*(bi->ourprev ? &bi->ourprev->ournext : &ourfirst) = bi;
		*(ti->ournext ? &ti->ournext->ourprev : &ourlast) = ti;
	}
 	while ((ti = bi->ournext) && bi->amount < ti->amount) {
		bi->ournext = ti->ournext;
		ti->ourprev = bi->ourprev;
		bi->ourprev = ti;
		ti->ournext = bi;
		*(bi->ournext ? &bi->ournext->ourprev : &ourlast) = bi;
		*(ti->ourprev ? &ti->ourprev->ournext : &ourfirst) = ti;
	}
 }

static inline void add_ourblock(struct block_internal *nodeBlock)
{
	nodeBlock->ourprev = ourlast;
	*(ourlast ? &ourlast->ournext : &ourfirst) = nodeBlock;
	ourlast = nodeBlock;
}

static inline void remove_ourblock(struct block_internal *nodeBlock){
	struct block_internal *prev = nodeBlock->ourprev, *next = nodeBlock->ournext;
	*(prev ? &prev->ournext : &ourfirst) = next;
	*(next ? &next->ourprev : &ourlast) = prev;
}
