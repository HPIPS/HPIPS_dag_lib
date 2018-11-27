/* транспорт, T13.654-T14.309 $DVS:time$ */

#ifndef XDAG_TRANSPORT_H
#define XDAG_TRANSPORT_H

#include <time.h>
#include <stdint.h>
#include <pthread.h>
#include "block.h"
#include "storage.h"

enum xdag_transport_flags {
	DAG_DAEMON = 1,
};

#ifdef __cplusplus
extern "C" {
#endif
	
/* 启动传输系统；bindto-ip：用于外部连接的套接字的端口
 * addr-port_.s-指向字符串的指针数组，该字符串具有用于连接的其他主机的参数（ip:port），
 * 字符串-计数
 * 传输线程数
 */
extern int dag_transport_start(int flags, int nthreads, const char *bindto, int npairs, const char **addr_port_pairs);

/* 生成具有随机数据的数组 */
extern int dag_generate_random_array(void *array, unsigned long size);

/*向网络发送新的块*/
extern int dag_send_new_block(struct xdag_block *b);

/* 请求来自远程主机的所有块，这些块在指定的时间间隔内；
 * 为每个块调用callback()，回调接收块和数据作为参数；
 * 在错误的情况下返回1
 */
extern int dag_request_blocks(xtime_t start_time, xtime_t end_time, void *data,
									void *(*callback)(void *, void *));

/* 用另一个主机的散列请求块 */
extern int dag_request_block(xdag_hash_t hash, void *conn);

/* 请求来自远程主机的块，并将块的总和放入“和”数组中， 
 * 块按从开始时间到结束时间的间隔过滤，分成16个部分；
 * 结束启动应该在表格16 ^ k中
 * (original russian comment is unclear too) */
extern int dag_request_sums(xtime_t start_time, xtime_t end_time, struct xdag_storage_sum sums[16]);

/* 执行传输级命令，输出流以显示命令执行的结果 */
extern int dag_net_command(const char *cmd, void *out);

/* 发送包，CONN与函数dnet_send_xdag_packet包相同 */
extern int dag_send_packet(struct xdag_block *b, void *conn);

/* 查看 dnet_user_crypt_action */
extern int dag_user_crypt_action(unsigned *data, unsigned long long data_id, unsigned size, int action);

extern pthread_mutex_t g_transport_mutex;
extern time_t g_dag_last_received;
	
#ifdef __cplusplus
};
#endif

#endif
