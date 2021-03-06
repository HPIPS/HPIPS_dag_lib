/* синхронизация, T13.738-T14.582 $DVS:time$ */

#ifndef XDAG_SYNC_H
#define XDAG_SYNC_H

#include "block.h"

#ifdef __cplusplus
extern "C" {
#endif

extern int g_dag_sync_on;
	
/*检查一个块并将其同步地包括在数据库中，如果出现错误，ruturs非零值*/
extern int dag_sync_add_block(struct dag_block *b, void *conn);

/*通知找到块的同步机制*/
extern int dag_sync_pop_block(struct dag_block *b);

/*初始化块同步*/
extern int dag_sync_init(void);
	
#ifdef __cplusplus
};
#endif

#endif
