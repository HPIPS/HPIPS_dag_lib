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

// ����鴦��Ŀ�ʼ
extern int dag_blocks_start(int is_pool, int mining_threads_count, int miner_address);

// ��鲢��洢������ӿ顣�ڴ��������·��ط���ֵ��
extern int dag_add_block(struct xdag_block *b);

// �������ǵĳ�ʼ�顣���û�п飬�򴴽���һ���顣
extern int dag_get_our_block(xdag_hash_t hash);

// ����ԭʼ��Ļص�
extern int dag_traverse_our_blocks(void *data,
	int (*callback)(void*, xdag_hash_t, xdag_amount_t, xtime_t, int));

// �������п�Ļص�
extern int dag_traverse_all_blocks(void *data, int (*callback)(void *data, xdag_hash_t hash,
	xdag_amount_t amount, xtime_t time));

// �����µĿ�
extern struct xdag_block* dag_create_block(struct xdag_field *fields, int inputsCount, int outputsCount, int hasRemark, 
	xdag_amount_t fee, xtime_t send_time, xdag_hash_t block_hash_result);

// ����������һ����
extern int dag_create_and_send_block(struct xdag_field *fields, int inputsCount, int outputsCount, int hasRemark, 
	xdag_amount_t fee, xtime_t send_time, xdag_hash_t block_hash_result);

// ����ָ����ַ������������ֻ����
extern xdag_amount_t dag_get_balance(xdag_hash_t hash);

// ����ָ����ַ�ĵ�ǰ���
extern int dag_set_balance(xdag_hash_t hash, xdag_amount_t balance);

// ��ָ������Ҫ�������㵱ǰ��Ӧ��
extern xdag_amount_t dag_get_supply(uint64_t nmain);

// ͨ����ϣ���ؿ��λ�ú�ʱ��; ���block��extra����block��= 0Ҳ����������
extern int64_t dag_get_block_pos(const xdag_hash_t hash, xtime_t *time, struct xdag_block *block);

// ����״̬��Ϣ�ַ���
extern const char* dag_get_block_state_info(uint8_t flag);

// ���ص�ǰʱ�ε�����������Ϊ64��
extern xtime_t dag_main_time(void);

// ���������翪ʼ��Ӧ��ʱ��εı��
extern xtime_t dag_start_main_time(void);

// ͨ�����ɢ�з���һЩ��������鲻�����ǵģ��򷵻�-1
extern int dag_get_key(xdag_hash_t hash);

// �鴦������³�ʼ��
extern int dag_blocks_reset(void);

// ��ӡ�йؿ����ϸ��Ϣ
extern int dag_print_block_info(xdag_hash_t hash, FILE *out);

// ��ӡN�������Ҫ����б�
extern void dag_list_main_blocks(int count, int print_only_addresses, FILE *out);

// ��ӡ��ǰ���ھ��N��������б�
extern void dag_list_mined_blocks(int count, int include_non_payed, FILE *out);

// ��ȡָ����ַ�����н��ף������ؽ�������
extern int dag_get_transactions(xdag_hash_t hash, void *data, int (*callback)(void*, int, int, xdag_hash_t, xdag_amount_t, xtime_t, const char*));

// ��ӡorphan��
void dag_list_orphan_blocks(int, FILE*);

// ��ɿ�Ĺ���
void dag_block_finish(void);
	
// ��ȡָ����ַ�Ŀ���Ϣ
extern int dag_get_block_info(xdag_hash_t, void *, int (*)(void*, int, xdag_hash_t, xdag_amount_t, xtime_t, const char*),
							void *, int (*)(void*, const char *, xdag_hash_t, xdag_amount_t));

#ifdef __cplusplus
};
#endif

#endif
