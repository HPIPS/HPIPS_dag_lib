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

// ����鴦��Ŀ�ʼ
extern int dag_blocks_start(int is_pool, int mining_threads_count, int miner_address);

// ��鲢��洢������ӿ顣�ڴ��������·��ط���ֵ��
extern int dag_add_block(struct dag_block *b);

// �������ǵĳ�ʼ�顣���û�п飬�򴴽���һ���顣
extern int dag_get_our_block(dag_hash_t hash);

// ����ԭʼ��Ļص�
extern int dag_traverse_our_blocks(void *data,
	int (*callback)(void*, dag_hash_t, dag_amount_t, xtime_t, int));

// �������п�Ļص�
extern int dag_traverse_all_blocks(void *data, int (*callback)(void *data, dag_hash_t hash,
	dag_amount_t amount, xtime_t time));

// �����µĿ�
extern struct dag_block* dag_create_block(struct dag_field *fields, int inputsCount, int outputsCount, int hasRemark, 
	dag_amount_t fee, xtime_t send_time, dag_hash_t block_hash_result);

// ����������һ����
extern int dag_create_and_send_block(struct dag_field *fields, int inputsCount, int outputsCount, int hasRemark, 
	dag_amount_t fee, xtime_t send_time, dag_hash_t block_hash_result);

// ����ָ����ַ������������ֻ����
extern dag_amount_t dag_get_balance(dag_hash_t hash);

// ����ָ����ַ�ĵ�ǰ���
extern int dag_set_balance(dag_hash_t hash, dag_amount_t balance);

// ��ָ������Ҫ�������㵱ǰ��Ӧ��
extern dag_amount_t dag_get_supply(uint64_t nmain);

// ͨ����ϣ���ؿ��λ�ú�ʱ��; ���block��extra����block��= 0Ҳ����������
extern int64_t dag_get_block_pos(const dag_hash_t hash, xtime_t *time, struct dag_block *block);

// ����״̬��Ϣ�ַ���
extern const char* dag_get_block_state_info(uint8_t flag);

// ���ص�ǰʱ�ε�����������Ϊ64��
extern xtime_t dag_main_time(void);

// ���������翪ʼ��Ӧ��ʱ��εı��
extern xtime_t dag_start_main_time(void);

// ͨ�����ɢ�з���һЩ��������鲻�����ǵģ��򷵻�-1
extern int dag_get_key(dag_hash_t hash);

// �鴦������³�ʼ��
extern int dag_blocks_reset(void);

// ��ӡ�йؿ����ϸ��Ϣ
extern int dag_print_block_info(dag_hash_t hash, FILE *out);

// ��ӡN�������Ҫ����б�
extern void dag_list_main_blocks(int count, int print_only_addresses, FILE *out);

// ��ӡ��ǰ���ھ��N��������б�
extern void dag_list_mined_blocks(int count, int include_non_payed, FILE *out);

// ��ȡָ����ַ�����н��ף������ؽ�������
extern int dag_get_transactions(dag_hash_t hash, void *data, int (*callback)(void*, int, int, dag_hash_t, dag_amount_t, xtime_t, const char*));

// ��ӡorphan��
void dag_list_orphan_blocks(int, FILE*);

// ��ɿ�Ĺ���
void dag_block_finish(void);
	
// ��ȡָ����ַ�Ŀ���Ϣ
//extern int dag_get_block_info(xdag_hash_t, void *, int (*)(void*, int, dag_hash_t, dag_amount_t, xtime_t, const char*),
//							void *, int (*)(void*, const char *, dag_hash_t, dag_amount_t));

#ifdef __cplusplus
};
#endif

#endif
