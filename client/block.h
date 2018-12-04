/* block processing, T13.654-T14.618 $DVS:time$ */

#ifndef DAG_BLOCK_H
#define DAG_BLOCK_H

#include <stdint.h>
#include <stdio.h>
#include <limits.h>
#include "hash.h"
#include "system.h"
#include "types.h"

enum dag_field_type {
	DAG_FIELD_NONCE,        //0
	DAG_FIELD_HEAD,         //1
	DAG_FIELD_IN,           //2
	DAG_FIELD_OUT,          //3
	DAG_FIELD_SIGN_IN,      //4
	DAG_FIELD_SIGN_OUT,     //5
	DAG_FIELD_PUBLIC_KEY_0, //6
	DAG_FIELD_PUBLIC_KEY_1, //7
	DAG_FIELD_HEAD_TEST,    //8
	DAG_FIELD_REMARK,       //9
	DAG_FIELD_RESERVE1,     //A
	DAG_FIELD_RESERVE2,     //B
	DAG_FIELD_RESERVE3,     //C
	DAG_FIELD_RESERVE4,     //D
	DAG_FIELD_RESERVE5,     //E
	DAG_FIELD_RESERVE6      //F
};

enum dag_message_type {
	DAG_MESSAGE_BLOCKS_REQUEST,
	DAG_MESSAGE_BLOCKS_REPLY,
	DAG_MESSAGE_SUMS_REQUEST,
	DAG_MESSAGE_SUMS_REPLY,
	DAG_MESSAGE_BLOCKEXT_REQUEST,
	DAG_MESSAGE_BLOCKEXT_REPLY,
	DAG_MESSAGE_BLOCK_REQUEST,
};

enum bi_flags {
	BI_MAIN       = 0x01,
	BI_MAIN_CHAIN = 0x02,
	BI_APPLIED    = 0x04,
	BI_MAIN_REF   = 0x08,
	BI_REF        = 0x10,
	BI_OURS       = 0x20,
	BI_EXTRA      = 0x40,
	BI_REMARK     = 0x80
};

#define DAG_BLOCK_FIELDS 16

#define REMARK_ENABLED 0

#if CHAR_BIT != 8
#error Your system hasn't exactly 8 bit for a char, it won't run.
#endif

typedef uint8_t dag_remark_t[32];

struct dag_field {
	union {
		struct {
			union {
				struct {
					uint64_t transport_header;
					uint64_t type;
					xtime_t time;
				};
				dag_hashlow_t hash;
			};
			union {
				dag_amount_t amount;
				xtime_t end_time;
			};
		};
		dag_hash_t data;
		dag_remark_t remark;
	};
};

struct dag_block {
	struct dag_field field[DAG_BLOCK_FIELDS];
};

#define dag_type(b, n) ((b)->field[0].type >> ((n) << 2) & 0xf)

#ifdef __cplusplus
extern "C" {
#endif

extern int g_bi_index_enable;

// 常规块处理的开始
extern int dag_blocks_start(int is_pool, int mining_threads_count, int miner_address);

// 检查并向存储块中添加块。在错误的情况下返回非零值。
extern int dag_add_block(struct dag_block *b);

// 返回我们的初始块。如果没有块，则创建第一个块。
extern int dag_get_our_block(dag_hash_t hash);

// 调用原始块的回调
extern int dag_traverse_our_blocks(void *data,
	int (*callback)(void*, dag_hash_t, dag_amount_t, xtime_t, int));

// 调用所有块的回调
extern int dag_traverse_all_blocks(void *data, int (*callback)(void *data, dag_hash_t hash,
	dag_amount_t amount, xtime_t time));

// 创建新的块
extern struct dag_block* dag_create_block(struct dag_field *fields, int inputsCount, int outputsCount, int hasRemark, 
	dag_amount_t fee, xtime_t send_time, dag_hash_t block_hash_result);

// 创建并发布一个块
extern int dag_create_and_send_block(struct dag_field *fields, int inputsCount, int outputsCount, int hasRemark, 
	dag_amount_t fee, xtime_t send_time, dag_hash_t block_hash_result);

// 返回指定地址的余额如果返回只是零
extern dag_amount_t dag_get_balance(dag_hash_t hash);

// 设置指定地址的当前余额
extern int dag_set_balance(dag_hash_t hash, dag_amount_t balance);

// 按指定的主要块数计算当前供应量
extern dag_amount_t dag_get_supply(uint64_t nmain);

// 通过哈希返回块的位置和时间; 如果block是extra并且block！= 0也返回整个块
extern int64_t dag_get_block_pos(const dag_hash_t hash, xtime_t *time, struct dag_block *block);

// 返回状态信息字符串
extern const char* dag_get_block_state_info(uint8_t flag);

// 返回当前时段的数量，周期为64秒
extern xtime_t dag_main_time(void);

// 返回与网络开始对应的时间段的编号
extern xtime_t dag_start_main_time(void);

// 通过块的散列返回一些键，如果块不是我们的，则返回-1
extern int dag_get_key(dag_hash_t hash);

// 块处理的重新初始化
extern int dag_blocks_reset(void);

// 打印有关块的详细信息
extern int dag_print_block_info(dag_hash_t hash, FILE *out);

// 打印N个最后主要块的列表
extern void dag_list_main_blocks(int count, int print_only_addresses, FILE *out);

// 打印当前池挖掘的N个最后块的列表
extern void dag_list_mined_blocks(int count, int include_non_payed, FILE *out);

// 获取指定地址的所有交易，并返回交易总数
extern int dag_get_transactions(dag_hash_t hash, void *data, int (*callback)(void*, int, int, dag_hash_t, dag_amount_t, xtime_t, const char*));

// 打印orphan块
void dag_list_orphan_blocks(int, FILE*);

// 完成块的工作
void dag_block_finish(void);
	
// 获取指定地址的块信息
//extern int dag_get_block_info(xdag_hash_t, void *, int (*)(void*, int, dag_hash_t, dag_amount_t, xtime_t, const char*),
//							void *, int (*)(void*, const char *, dag_hash_t, dag_amount_t));

#ifdef __cplusplus
};
#endif

#endif
