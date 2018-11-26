/* block processing, T13.654-T14.618 $DVS:time$ */

#ifndef XDAG_BLOCK_H
#define XDAG_BLOCK_H

#include <stdint.h>
#include <stdio.h>
#include <limits.h>
#include "hash.h"
#include "system.h"
#include "types.h"

enum dag_field_type {
	XDAG_FIELD_NONCE,        //0
	XDAG_FIELD_HEAD,         //1
	XDAG_FIELD_IN,           //2
	XDAG_FIELD_OUT,          //3
	XDAG_FIELD_SIGN_IN,      //4
	XDAG_FIELD_SIGN_OUT,     //5
	XDAG_FIELD_PUBLIC_KEY_0, //6
	XDAG_FIELD_PUBLIC_KEY_1, //7
	XDAG_FIELD_HEAD_TEST,    //8
	XDAG_FIELD_REMARK,       //9
	XDAG_FIELD_RESERVE1,     //A
	XDAG_FIELD_RESERVE2,     //B
	XDAG_FIELD_RESERVE3,     //C
	XDAG_FIELD_RESERVE4,     //D
	XDAG_FIELD_RESERVE5,     //E
	XDAG_FIELD_RESERVE6      //F
};

enum dag_message_type {
	XDAG_MESSAGE_BLOCKS_REQUEST,
	XDAG_MESSAGE_BLOCKS_REPLY,
	XDAG_MESSAGE_SUMS_REQUEST,
	XDAG_MESSAGE_SUMS_REPLY,
	XDAG_MESSAGE_BLOCKEXT_REQUEST,
	XDAG_MESSAGE_BLOCKEXT_REPLY,
	XDAG_MESSAGE_BLOCK_REQUEST,
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

typedef uint8_t xdag_remark_t[32];

struct xdag_field {
	union {
		struct {
			union {
				struct {
					uint64_t transport_header;
					uint64_t type;
					xtime_t time;
				};
				xdag_hashlow_t hash;
			};
			union {
				xdag_amount_t amount;
				xtime_t end_time;
			};
		};
		xdag_hash_t data;
		xdag_remark_t remark;
	};
};

struct xdag_block {
	struct xdag_field field[DAG_BLOCK_FIELDS];
};

#define dag_type(b, n) ((b)->field[0].type >> ((n) << 2) & 0xf)

#ifdef __cplusplus
extern "C" {
#endif

extern int g_bi_index_enable;

// 常规块处理的开始
extern int dag_blocks_start(int is_pool, int mining_threads_count, int miner_address);

// 检查并向存储块中添加块。在错误的情况下返回非零值。
extern int dag_add_block(struct xdag_block *b);

// 返回我们的初始块。如果没有块，则创建第一个块。
extern int dag_get_our_block(xdag_hash_t hash);

// 调用原始块的回调
extern int dag_traverse_our_blocks(void *data,
	int (*callback)(void*, xdag_hash_t, xdag_amount_t, xtime_t, int));

// 调用所有块的回调
extern int dag_traverse_all_blocks(void *data, int (*callback)(void *data, xdag_hash_t hash,
	xdag_amount_t amount, xtime_t time));

// 创建新的块
extern struct xdag_block* dag_create_block(struct xdag_field *fields, int inputsCount, int outputsCount, int hasRemark, 
	xdag_amount_t fee, xtime_t send_time, xdag_hash_t block_hash_result);

// 创建并发布一个块
extern int dag_create_and_send_block(struct xdag_field *fields, int inputsCount, int outputsCount, int hasRemark, 
	xdag_amount_t fee, xtime_t send_time, xdag_hash_t block_hash_result);

// 返回指定地址的余额如果返回只是零
extern xdag_amount_t dag_get_balance(xdag_hash_t hash);

// 设置指定地址的当前余额
extern int dag_set_balance(xdag_hash_t hash, xdag_amount_t balance);

// 按指定的主要块数计算当前供应量
extern xdag_amount_t dag_get_supply(uint64_t nmain);

// 通过哈希返回块的位置和时间; 如果block是extra并且block！= 0也返回整个块
extern int64_t dag_get_block_pos(const xdag_hash_t hash, xtime_t *time, struct xdag_block *block);

// 返回状态信息字符串
extern const char* dag_get_block_state_info(uint8_t flag);

// 返回当前时段的数量，周期为64秒
extern xtime_t dag_main_time(void);

// 返回与网络开始对应的时间段的编号
extern xtime_t dag_start_main_time(void);

// 通过块的散列返回一些键，如果块不是我们的，则返回-1
extern int dag_get_key(xdag_hash_t hash);

// 块处理的重新初始化
extern int dag_blocks_reset(void);

// 打印有关块的详细信息
extern int dag_print_block_info(xdag_hash_t hash, FILE *out);

// 打印N个最后主要块的列表
extern void dag_list_main_blocks(int count, int print_only_addresses, FILE *out);

// 打印当前池挖掘的N个最后块的列表
extern void dag_list_mined_blocks(int count, int include_non_payed, FILE *out);

// 获取指定地址的所有交易，并返回交易总数
extern int dag_get_transactions(xdag_hash_t hash, void *data, int (*callback)(void*, int, int, xdag_hash_t, xdag_amount_t, xtime_t, const char*));

// 打印orphan块
void dag_list_orphan_blocks(int, FILE*);

// 完成块的工作
void dag_block_finish(void);
	
// 获取指定地址的块信息
extern int dag_get_block_info(xdag_hash_t, void *, int (*)(void*, int, xdag_hash_t, xdag_amount_t, xtime_t, const char*),
							void *, int (*)(void*, const char *, xdag_hash_t, xdag_amount_t));

#ifdef __cplusplus
};
#endif

#endif
