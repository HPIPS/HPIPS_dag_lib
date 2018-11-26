/* локальное хранилище, T13.663-T13.788 $DVS:time$ */

#ifndef DAG_STORAGE_H
#define DAG_STORAGE_H

#include "block.h"

struct dag_storage_sum {
	uint64_t sum;
	uint64_t size;
};

#ifdef __cplusplus
extern "C" {
#endif
	
/* 将块保存到本地存储，在错误的情况下返回其编号或-1 */
extern int64_t dag_storage_save(const struct dag_block *b);

/*从本地存储库读取块及其编号；将其写入缓冲区或返回永久引用，万一出错，返回0*/
extern struct dag_block *dag_storage_load(dag_hash_t hash, xtime_t time, uint64_t pos,
	struct dag_block *buf);

/* 调用存储库中指定时间间隔的所有块的回调；返回块的数量*/
extern uint64_t dag_load_blocks(xtime_t start_time, xtime_t end_time, void *data,
									  void *(*callback)(void *block, void *data));

/* 将块和置于“和”数组中，按从开始时间到结束时间的间隔对块进行过滤，分成16个部分； 
 * 结束启动应该在表格16 ^ k中 
 * (original russian comment is unclear too) */
extern int dag_load_sums(xtime_t start_time, xtime_t end_time, struct dag_storage_sum sums[16]);

/* 用存储完成工作 */
extern void dag_storage_finish(void);

#ifdef __cplusplus
};
#endif
		
#endif
