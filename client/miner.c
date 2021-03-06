/* пул и майнер, T13.744-T14.390 $DVS:time$ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "system.h"
#include "../dus/programs/dfstools/source/dfslib/dfslib_crypt.h"
#include "../dus/programs/dar/source/include/crc.h"
#include "address.h"
#include "block.h"
#include "init.h"
#include "miner.h"
#include "storage.h"
#include "sync.h"
#include "transport.h"
#include "mining_common.h"
#include "network.h"
#include "utils/log.h"
#include "utils/utils.h"

#define MINERS_PWD             "minersgonnamine"
#define SECTOR0_BASE           0x1947f3acu
#define SECTOR0_OFFSET         0x82e9d1b5u
#define SEND_PERIOD            10                                  /* share period of sending shares */
#define POOL_LIST_FILE         (g_dag_testnet ? "pools-testnet.txt" : "pools.txt")

struct miner {
	struct dag_field id;
	uint64_t nfield_in;
	uint64_t nfield_out;
};

static struct miner g_local_miner;
static pthread_mutex_t g_miner_mutex = PTHREAD_MUTEX_INITIALIZER;

/* a number of mining threads */
int g_dag_mining_threads = 0;

static int g_socket = -1, g_stop_mining = 1;

static int can_send_share(time_t current_time, time_t task_time, time_t share_time)
{
	int can_send = current_time - share_time >= SEND_PERIOD && current_time - task_time <= 64;
	if(g_dag_mining_threads == 0 && share_time >= task_time) {
		can_send = 0;  //we send only one share per task if mining is turned off
	}
	return can_send;
}

/* 连接矿工到池的初始化  */
extern int dag_initialize_miner(const char *pool_address)
{
	pthread_t th;

	memset(&g_local_miner, 0, sizeof(struct miner));
	dag_get_our_block(g_local_miner.id.data);

	int err = pthread_create(&th, 0, miner_net_thread, (void*)pool_address);
	if(err != 0) {
		printf("创建 miner_net_thread 失败, error : %s\n", strerror(err));
		return -1;
	}

	err = pthread_detach(th);
	if(err != 0) {
		printf("分离 miner_net_thread 失败, error : %s\n", strerror(err));
		//return -1; //fixme: not sure why pthread_detach return 3
	}

	return 0;
}

static int send_to_pool(struct dag_field *fld, int nfld)
{
	struct dag_field f[DAG_BLOCK_FIELDS];
	dag_hash_t h;
	struct miner *m = &g_local_miner;
	int todo = nfld * sizeof(struct dag_field), done = 0;

	if(g_socket < 0) {
		return -1;
	}

	memcpy(f, fld, todo);

	if(nfld == DAG_BLOCK_FIELDS) {
		f[0].transport_header = 0;

		dag_hash(f, sizeof(struct dag_block), h);

		f[0].transport_header = BLOCK_HEADER_WORD;

		uint32_t crc = crc_of_array((uint8_t*)f, sizeof(struct dag_block));

		f[0].transport_header |= (uint64_t)crc << 32;
	}

	for(int i = 0; i < nfld; ++i) {
		dfslib_encrypt_array(g_crypt, (uint32_t*)(f + i), DATA_SIZE, m->nfield_out++);
	}

	while(todo) {
		struct pollfd p;

		p.fd = g_socket;
		p.events = POLLOUT;

		if(!poll(&p, 1, 1000)) continue;

		if(p.revents & (POLLHUP | POLLERR)) {
			return -1;
		}

		if(!(p.revents & POLLOUT)) continue;

		int res = write(g_socket, (uint8_t*)f + done, todo);
		if(res <= 0) {
			return -1;
		}

		done += res;
		todo -= res;
	}

	if(nfld == DAG_BLOCK_FIELDS) {
		dag_info("Sent  : %016llx%016llx%016llx%016llx t=%llx res=%d",
			h[3], h[2], h[1], h[0], fld[0].time, 0);
	}

	return 0;
}

void *miner_net_thread(void *arg)
{
	struct dag_block b;
	struct dag_field data[2];
	dag_hash_t hash;
	const char *pool_address = (const char*)arg;
	const char *mess = NULL;
	int res = 0;
	xtime_t t;
	struct miner *m = &g_local_miner;

	while(!g_dag_sync_on) {
		sleep(1);
	}

begin:
	m->nfield_in = m->nfield_out = 0;

	int ndata = 0;
	int maxndata = sizeof(struct dag_field);
	time_t share_time = 0;
	time_t task_time = 0;

	if(g_miner_address) {
		if(dag_address2hash(g_miner_address, hash)) {
			mess = "矿工地址不正确";
			goto err;
		}
	} else if(dag_get_our_block(hash)) {
		mess = "无法创建块 ";
		goto err;
	}

	const int64_t pos = dag_get_block_pos(hash, &t, &b);
	if (pos == -2l) {
		;
	} else if (pos < 0) {
		mess = "can't find the block";
		goto err;
	} else {
		struct dag_block *blk = dag_storage_load(hash, t, pos, &b);
		if(!blk) {
			mess = "can't load the block";
			goto err;
		}
		if(blk != &b) memcpy(&b, blk, sizeof(struct dag_block));
	}

	pthread_mutex_lock(&g_miner_mutex);
	g_socket = dag_connect_pool(pool_address, &mess);
	if(g_socket == INVALID_SOCKET) {
		pthread_mutex_unlock(&g_miner_mutex);
		goto err;
	}

	if(send_to_pool(b.field, DAG_BLOCK_FIELDS) < 0) {
		mess = "socket is closed";
		pthread_mutex_unlock(&g_miner_mutex);
		goto err;
	}
	pthread_mutex_unlock(&g_miner_mutex);

	for(;;) {
		struct pollfd p;

		pthread_mutex_lock(&g_miner_mutex);

		if(g_socket < 0) {
			pthread_mutex_unlock(&g_miner_mutex);
			mess = "socket is closed";
			goto err;
		}

		p.fd = g_socket;
		time_t current_time = time(0);
		p.events = POLLIN | (can_send_share(current_time, task_time, share_time) ? POLLOUT : 0);

		if(!poll(&p, 1, 0)) {
			pthread_mutex_unlock(&g_miner_mutex);
			sleep(1);
			continue;
		}

		if(p.revents & POLLHUP) {
			pthread_mutex_unlock(&g_miner_mutex);
			mess = "socket hangup";
			goto err;
		}

		if(p.revents & POLLERR) {
			pthread_mutex_unlock(&g_miner_mutex);
			mess = "socket error";
			goto err;
		}

		if(p.revents & POLLIN) {
			res = read(g_socket, (uint8_t*)data + ndata, maxndata - ndata);
			if(res < 0) {
				pthread_mutex_unlock(&g_miner_mutex); mess = "read error on socket"; goto err;
			}
			ndata += res;
			if(ndata == maxndata) {
				struct dag_field *last = data + (ndata / sizeof(struct dag_field) - 1);

				dfslib_uncrypt_array(g_crypt, (uint32_t*)last->data, DATA_SIZE, m->nfield_in++);

				if(!memcmp(last->data, hash, sizeof(dag_hashlow_t))) {
					dag_set_balance(hash, last->amount);

					pthread_mutex_lock(&g_transport_mutex);
					g_dag_last_received = current_time;
					pthread_mutex_unlock(&g_transport_mutex);

					ndata = 0;

					maxndata = sizeof(struct dag_field);
				} else if(maxndata == 2 * sizeof(struct dag_field)) {
					const uint64_t task_index = g_dag_pool_task_index + 1;
					struct dag_pool_task *task = &g_dag_pool_task[task_index & 1];

					task->task_time = dag_main_time();
					dag_hash_set_state(task->ctx, data[0].data,
						sizeof(struct dag_block) - 2 * sizeof(struct dag_field));
					dag_hash_update(task->ctx, data[1].data, sizeof(struct dag_field));
					dag_hash_update(task->ctx, hash, sizeof(dag_hashlow_t));

					dag_generate_random_array(task->nonce.data, sizeof(dag_hash_t));

					memcpy(task->nonce.data, hash, sizeof(dag_hashlow_t));
					memcpy(task->lastfield.data, task->nonce.data, sizeof(dag_hash_t));

					dag_hash_final(task->ctx, &task->nonce.amount, sizeof(uint64_t), task->minhash.data);

					g_dag_pool_task_index = task_index;
					task_time = time(0);

					dag_info("Task  : t=%llx N=%llu", task->task_time << 16 | 0xffff, task_index);

					ndata = 0;
					maxndata = sizeof(struct dag_field);
				} else {
					maxndata = 2 * sizeof(struct dag_field);
				}
			}
		}

		if(p.revents & POLLOUT) {
			const uint64_t task_index = g_dag_pool_task_index;
			struct dag_pool_task *task = &g_dag_pool_task[task_index & 1];
			uint64_t *h = task->minhash.data;

			share_time = time(0);
			res = send_to_pool(&task->lastfield, 1);
			pthread_mutex_unlock(&g_miner_mutex);

			dag_info("Share : %016llx%016llx%016llx%016llx t=%llx res=%d",
				h[3], h[2], h[1], h[0], task->task_time << 16 | 0xffff, res);

			if(res) {
				mess = "write error on socket"; goto err;
			}
		} else {
			pthread_mutex_unlock(&g_miner_mutex);
		}
	}

	return 0;

err:
	dag_err("Miner: %s (error %d)", mess, res);

	pthread_mutex_lock(&g_miner_mutex);

	if(g_socket != INVALID_SOCKET) {
		close(g_socket);
		g_socket = INVALID_SOCKET;
	}

	pthread_mutex_unlock(&g_miner_mutex);

	sleep(5);

	goto begin;
}

static void *mining_thread(void *arg)
{
	dag_hash_t hash;
	struct dag_field last;
	const int nthread = (int)(uintptr_t)arg;
	uint64_t oldntask = 0;
	uint64_t nonce;

	while(!g_dag_sync_on && !g_stop_mining) {
		sleep(1);
	}

	while(!g_stop_mining) {
		const uint64_t ntask = g_dag_pool_task_index;
		struct dag_pool_task *task = &g_dag_pool_task[ntask & 1];

		if(!ntask) {
			sleep(1);
			continue;
		}

		if(ntask != oldntask) {
			oldntask = ntask;
			memcpy(last.data, task->nonce.data, sizeof(dag_hash_t));
			nonce = last.amount + nthread;
		}

		last.amount = dag_hash_final_multi(task->ctx, &nonce, 4096, g_dag_mining_threads, hash);
		g_dag_extstats.nhashes += 4096;

		dag_set_min_share(task, last.data, hash);
	}

	return 0;
}

/* 更改挖掘线程的数量 */
int dag_mining_start(int n_mining_threads)
{
	pthread_t th;

	if(n_mining_threads == g_dag_mining_threads) {

	} else if(!n_mining_threads) {
		g_stop_mining = 1;
		g_dag_mining_threads = 0;
	} else if(!g_dag_mining_threads) {
		g_stop_mining = 0;
	} else if(g_dag_mining_threads > n_mining_threads) {
		g_stop_mining = 1;
		sleep(5);
		g_stop_mining = 0;
		g_dag_mining_threads = 0;
	}

	while(g_dag_mining_threads < n_mining_threads) {
		g_dag_mining_threads++;
		int err = pthread_create(&th, 0, mining_thread, (void*)(uintptr_t)g_dag_mining_threads);
		if(err != 0) {
			printf("创建MungIn线程失败, error : %s\n", strerror(err));
			continue;
		}

		err = pthread_detach(th);
		if(err != 0) {
			printf("分离挖掘线程失败, error : %s\n", strerror(err));
			continue;
		}
	}

	return 0;
}

/* 通过池向网络发送块 */
int dag_send_block_via_pool(struct dag_block *b)
{
	if(g_socket < 0) return -1;

	pthread_mutex_lock(&g_miner_mutex);
	int ret = send_to_pool(b->field, DAG_BLOCK_FIELDS);
	pthread_mutex_unlock(&g_miner_mutex);
	return ret;
}

/* 从池列表中选择随机池 */
int dag_pick_pool(char *pool_address)
{
	char addresses[30][50] = {0};
	const char *error_message;
	srand(time(NULL));
	
	int count = 0;
	FILE *fp = dag_open_file(POOL_LIST_FILE, "r");
	if(!fp) {
		printf("找不到池列表\n");
		return 0;
	}
	while(fgets(addresses[count], 50, fp)) {
		// remove trailing newline character
		addresses[count][strcspn(addresses[count], "\n")] = 0;
		++count;
	}
	fclose(fp);

	int start_index = count ? rand() % count : 0;
	int index = start_index;
	do {
		int socket = dag_connect_pool(addresses[index], &error_message);
		if(socket != INVALID_SOCKET) {
			dag_connection_close(socket);
			strncpy(pool_address, addresses[index], 49);
			return 1;
		} else {
			++index;
			if(index >= count) {
				index = 0;
			}
		}
	} while(index != start_index);

	printf("钱包无法连接到网络。检查网络连接\n");
	return 0;
}
