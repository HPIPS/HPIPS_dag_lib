/* локальное хранилище, T13.663-T14.596 $DVS:time$ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include "storage.h"
#include "init.h"
#include "hash.h"
#include "utils/log.h"
#include "utils/utils.h"

#define STORAGE_DIR0            "storage%s"
#define STORAGE_DIR0_ARGS(t)    (g_dag_testnet ? "-testnet" : "")
#define STORAGE_DIR1            STORAGE_DIR0 DELIMITER "%02x"
#define STORAGE_DIR1_ARGS(t)    STORAGE_DIR0_ARGS(t), (int)((t) >> 40)
#define STORAGE_DIR2            STORAGE_DIR1 DELIMITER "%02x"
#define STORAGE_DIR2_ARGS(t)    STORAGE_DIR1_ARGS(t), (int)((t) >> 32) & 0xff
#define STORAGE_DIR3            STORAGE_DIR2 DELIMITER "%02x"
#define STORAGE_DIR3_ARGS(t)    STORAGE_DIR2_ARGS(t), (int)((t) >> 24) & 0xff
#define STORAGE_FILE            STORAGE_DIR3 DELIMITER "%02x.dat"
#define STORAGE_FILE_ARGS(t)    STORAGE_DIR3_ARGS(t), (int)((t) >> 16) & 0xff
#define SUMS_FILE               "sums.dat"

static pthread_mutex_t storage_mutex = PTHREAD_MUTEX_INITIALIZER;
static int in_adding_all = 0;

static int correct_storage_sum(const char *path, int pos, const struct dag_storage_sum *sum, int add)
{
	struct dag_storage_sum sums[256];
	FILE *f = dag_open_file(path, "r+b");

	if (f) {
		if (fread(sums, sizeof(struct dag_storage_sum), 256, f) != 256) {
			dag_close_file(f); dag_err("Storage: sums file %s corrupted", path);
			return -1;
		}
		rewind(f);
	} else {
		f = dag_open_file(path, "wb");
		if (!f) {
			dag_err("Storage: can't create file %s", path);
			return -1;
		}
		memset(sums, 0, sizeof(sums));
	}

	if (!add) {
		if (sums[pos].size == sum->size && sums[pos].sum == sum->sum) {
			dag_close_file(f); return 0;
		}

		if (sums[pos].size || sums[pos].sum) {
			sums[pos].size = sums[pos].sum = 0;
			dag_err("Storage: corrupted, sums file %s, pos %x", path, pos);
		}
	}

	sums[pos].size += sum->size;
	sums[pos].sum += sum->sum;
	
	if (fwrite(sums, sizeof(struct dag_storage_sum), 256, f) != 256) {
		dag_close_file(f); dag_err("Storage: can't write file %s", path); return -1;
	}
	
	dag_close_file(f);
	
	return 1;
}

static int correct_storage_sums(xtime_t t, const struct dag_storage_sum *sum, int add)
{
	char path[256] = {0};

	sprintf(path, STORAGE_DIR3 DELIMITER SUMS_FILE, STORAGE_DIR3_ARGS(t));
	int res = correct_storage_sum(path, (t >> 16) & 0xff, sum, add);
	if (res <= 0) return res;
	
	sprintf(path, STORAGE_DIR2 DELIMITER SUMS_FILE, STORAGE_DIR2_ARGS(t));
	res = correct_storage_sum(path, (t >> 24) & 0xff, sum, 1);
	if (res <= 0) return res;
	
	sprintf(path, STORAGE_DIR1 DELIMITER SUMS_FILE, STORAGE_DIR1_ARGS(t));
	res = correct_storage_sum(path, (t >> 32) & 0xff, sum, 1);
	if (res <= 0) return res;
	
	sprintf(path, STORAGE_DIR0 DELIMITER SUMS_FILE, STORAGE_DIR0_ARGS(t));
	res = correct_storage_sum(path, (t >> 40) & 0xff, sum, 1);
	if (res <= 0) return res;
	
	return 0;
}

/* 将块保存到本地存储，在错误的情况下返回其编号或-1 */
int64_t dag_storage_save(const struct dag_block *b)
{
	struct dag_storage_sum s;
	char path[256] = {0};
	int64_t res;

	if (in_adding_all) {
		return -1;
	}
	
	sprintf(path, STORAGE_DIR0, STORAGE_DIR0_ARGS(b->field[0].time));
	dag_mkdir(path);
	
	sprintf(path, STORAGE_DIR1, STORAGE_DIR1_ARGS(b->field[0].time));
	dag_mkdir(path);
	
	sprintf(path, STORAGE_DIR2, STORAGE_DIR2_ARGS(b->field[0].time));
	dag_mkdir(path);
	
	sprintf(path, STORAGE_DIR3, STORAGE_DIR3_ARGS(b->field[0].time));
	dag_mkdir(path);
	
	sprintf(path, STORAGE_FILE, STORAGE_FILE_ARGS(b->field[0].time));
	
	pthread_mutex_lock(&storage_mutex);
	
	FILE *f = dag_open_file(path, "ab");
	if (f) {
		fseek(f, 0, SEEK_END);
		res = ftell(f);
		fwrite(b, sizeof(struct dag_block), 1, f);
		dag_close_file(f);
		s.size = sizeof(struct dag_block);
		s.sum = 0;

		for (int j = 0; j < sizeof(struct dag_block) / sizeof(uint64_t); ++j) {
			s.sum += ((uint64_t*)b)[j];
		}

		if (correct_storage_sums(b->field[0].time, &s, 1)) {
			res = -1;
		}
	} else {
		res = -1;
	}

	pthread_mutex_unlock(&storage_mutex);
	
	return res;
}

/* 从本地存储库读取块及其编号；将其写入缓冲区或返回永久引用，万一出错，返回0  */
struct dag_block *dag_storage_load(dag_hash_t hash, xtime_t time, uint64_t pos, struct dag_block *buf)
{
	dag_hash_t hash0;
	char path[256] = {0};

	sprintf(path, STORAGE_FILE, STORAGE_FILE_ARGS(time));

	pthread_mutex_lock(&storage_mutex);
	
	FILE *f = dag_open_file(path, "rb");
	if (f) {
		if (fseek(f, pos, SEEK_SET) < 0 || fread(buf, sizeof(struct dag_block), 1, f) != 1) {
			buf = 0;
		}
		dag_close_file(f);
	} else {
		buf = 0;
	}

	pthread_mutex_unlock(&storage_mutex);
	
	if (buf) {
		dag_hash(buf, sizeof(struct dag_block), hash0);
		if (memcmp(hash, hash0, sizeof(dag_hashlow_t))) {
			buf = 0;
		}
	}

	if (!buf) {
		dag_blocks_reset();
	}

	return buf;
}

#define bufsize (0x100000 / sizeof(struct dag_block))

static int sort_callback(const void *l, const void *r)
{
	struct dag_block **L = (struct dag_block **)l, **R = (struct dag_block **)r;

	if ((*L)->field[0].time < (*R)->field[0].time) return -1;
	if ((*L)->field[0].time > (*R)->field[0].time) return 1;

	return 0;
}

/* Calls a callback for all blocks from the repository that are in specified time interval; returns the number of blocks */
uint64_t dag_load_blocks(xtime_t start_time, xtime_t end_time, void *data, void *(*callback)(void *, void *))
{
	struct dag_block *buf, *pbuf[bufsize];
	struct dag_storage_sum s;
	char path[256] = {0};
	
	uint64_t sum = 0, pos = 0, mask;
	int64_t i, j, k, todo;

	s.size = s.sum = 0;

	buf = malloc(bufsize*sizeof(struct dag_block));
 	if(buf == NULL){
		dag_fatal("malloc failed [function xdag_load_blocks]");
		return 0;
	}

	while (start_time < end_time) {
		sprintf(path, STORAGE_FILE, STORAGE_FILE_ARGS(start_time));

		pthread_mutex_lock(&storage_mutex);
		
		FILE *f = dag_open_file(path, "rb");
		if (f) {
			if (fseek(f, pos, SEEK_SET) < 0) todo = 0;
			else todo = fread(buf, sizeof(struct dag_block), bufsize, f);
			dag_close_file(f);
		} else {
			todo = 0;
		}
		
		pthread_mutex_unlock(&storage_mutex);
		
		uint64_t pos0 = pos;

		for (i = k = 0; i < todo; ++i, pos += sizeof(struct dag_block)) {
			if (buf[i].field[0].time >= start_time && buf[i].field[0].time < end_time) {
				s.size += sizeof(struct dag_block);

				for (j = 0; j < sizeof(struct dag_block) / sizeof(uint64_t); ++j) {
					s.sum += ((uint64_t*)(buf + i))[j];
				}

				pbuf[k++] = buf + i;
			}
		}

		if (k) {
			qsort(pbuf, k, sizeof(struct dag_block *), sort_callback);
		}

		for (i = 0; i < k; ++i) {
			pbuf[i]->field[0].transport_header = pos0 + ((uint8_t*)pbuf[i] - (uint8_t*)buf);
			if (callback(pbuf[i], data)) {
				free(buf);
				return sum;
			}
			sum++;
		}

		if (todo != bufsize) {
			if (f) {
				pthread_mutex_lock(&storage_mutex);
				
				int res = correct_storage_sums(start_time, &s, 0);
				
				pthread_mutex_unlock(&storage_mutex);
				
				if (res) break;
				
				s.size = s.sum = 0;
				mask = (1l << 16) - 1;
			} else if (sprintf(path, STORAGE_DIR3, STORAGE_DIR3_ARGS(start_time)), dag_file_exists(path)) {
				mask = (1l << 16) - 1;
			} else if (sprintf(path, STORAGE_DIR2, STORAGE_DIR2_ARGS(start_time)), dag_file_exists(path)) {
				mask = (1l << 24) - 1;
			} else if (sprintf(path, STORAGE_DIR1, STORAGE_DIR1_ARGS(start_time)), dag_file_exists(path)) {
				mask = (1ll << 32) - 1;
			} else {
				mask = (1ll << 40) - 1;
			}

			start_time |= mask;
			start_time++;
			
			pos = 0;
		}
	}

	free(buf);
	return sum;
}

/* 将块和置于“和”数组中，按从开始时间到结束时间的间隔对块进行过滤，分成16个部分；
 * 结束启动应该在表格16 ^ k中
 * (original russian comment is unclear too) */
int dag_load_sums(xtime_t start_time, xtime_t end_time, struct dag_storage_sum sums[16])
{
	struct dag_storage_sum buf[256];
	char path[256] = {0};
	int i, level;

	end_time -= start_time;
	if (!end_time || end_time & (end_time - 1) || end_time & 0xFFFEEEEEEEEFFFFFl) return -1;

	for (level = -6; end_time; level++, end_time >>= 4);

	if (level < 2) {
		sprintf(path, STORAGE_DIR3 DELIMITER SUMS_FILE, STORAGE_DIR3_ARGS(start_time & 0xffffff000000l));
	} else if (level < 4) {
		sprintf(path, STORAGE_DIR2 DELIMITER SUMS_FILE, STORAGE_DIR2_ARGS(start_time & 0xffff00000000l));
	} else if (level < 6) {
		sprintf(path, STORAGE_DIR1 DELIMITER SUMS_FILE, STORAGE_DIR1_ARGS(start_time & 0xff0000000000l));
	} else {
		sprintf(path, STORAGE_DIR0 DELIMITER SUMS_FILE, STORAGE_DIR0_ARGS(start_time & 0x000000000000l));
	}

	FILE *f = dag_open_file(path, "rb");
	if (f) {
		fread(buf, sizeof(struct dag_storage_sum), 256, f); dag_close_file(f);
	} else {
		memset(buf, 0, sizeof(buf));
	}

	if (level & 1) {
		memset(sums, 0, 16 * sizeof(struct dag_storage_sum));

		for (i = 0; i < 256; ++i) {
			sums[i >> 4].size += buf[i].size, sums[i >> 4].sum += buf[i].sum;
		}
	} else {
		memcpy(sums, buf + (start_time >> ((level + 4) * 4) & 0xf0), 16 * sizeof(struct dag_storage_sum));
	}

	return 1;
}

/* 用存储完成工作 */
void dag_storage_finish(void)
{
	pthread_mutex_lock(&storage_mutex);
}
