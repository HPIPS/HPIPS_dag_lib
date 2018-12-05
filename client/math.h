/* math, T14.579-T14.618 $DVS:time$ */

#ifndef XDAG_MATH_H
#define XDAG_MATH_H

#include <stdint.h>
#include "types.h"
#include "system.h"
#include "hash.h"

#define xdag_amount2xdag(amount) ((unsigned)((amount) >> 32))
#define xdag_amount2cheato(amount) ((unsigned)(((uint64_t)(unsigned)(amount) * 1000000000) >> 32))

#ifdef __cplusplus
extern "C" {
#endif

// cheato ת���� dag
extern long double amount2xdags(dag_amount_t);

// dag ת���� cheato
extern dag_amount_t xdags2amount(const char*);

// calculate difficulty from hash
dag_diff_t xdag_hash_difficulty(dag_hash_t);

// convert diff to logarithm representation
long double xdag_diff2log(xdag_diff_t);

// convert difficulty represented as logarithm in hashrate
long double xdag_log_difficulty2hashrate(long double);

// ���ϴμ�¼��HASHRATE_LAST_MAX_TIME �Ѷȼ����ϣ��
// DIFFʵ������ָ��Ԫ�ص�xdAgDeffixt�����HASHRATE_LAST_MAX_TIMEָ��
long double dag_hashrate(dag_diff_t *diff);
	
#ifdef __cplusplus
};
#endif

#endif
