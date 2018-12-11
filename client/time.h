/* time functions, T14.524-T14.582 $DVS:time$ */
#ifndef DAG_TIME_H
#define DAG_TIME_H

#include <stdint.h>
#include "types.h"


/* the maximum period of time for which blocks are requested, not their amounts */
#define REQUEST_BLOCKS_MAX_TIME (1 << 20)

#define HASHRATE_LAST_MAX_TIME  (64 * 4) // numbers of main blocks in about 4H, to calculate the pool and network mean hashrate
#define DEF_TIME_LIMIT          0 // (MAIN_CHAIN_PERIOD / 2)
#define MAIN_CHAIN_PERIOD       (64 << 10)
#define MAIN_TIME(t)            ((t) >> 16)
#define DAG_TEST_ERA           0x16900000000ll
#define DAG_MAIN_ERA           0x16940000000ll
#define DAG_ERA				   g_dag_era
#define MAX_TIME_NMAIN_STALLED  (1 << 10)

#ifdef __cplusplus
extern "C" {
#endif

extern xtime_t g_dag_era;
// 返回一个时间周期索引，其中周期为64秒长。
xtime_t dag_main_time(void);

//返回与网络启动相对应的时间周期索引
xtime_t dag_start_main_time(void);

//初始化时间模型
int dag_time_init(void);

//转换xTimeTyt到字符串表示
// 字符串缓冲区“BUF”的最小长度应该是60。
void dag_xtime_to_string(xtime_t time, char *buf);

// convert time_t to string representation
// minimal length of string buffer `buf` should be 50
void dag_time_to_string(xtime_t time, char* buf);

extern xtime_t dag_get_xtimestamp(void);

extern uint64_t dag_get_time_ms(void);
	
#ifdef __cplusplus
};
#endif

#endif
