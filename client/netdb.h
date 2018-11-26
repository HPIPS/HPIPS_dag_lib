/* hosts base, T13.714-T13.785 $DVS:time$ */

#ifndef XDAG_NETDB_H
#define XDAG_NETDB_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 防止自动刷新参数 */
extern int g_prevent_auto_refresh;

/* 初始化主机基础，“Url HoothStrut”-我们主机的外部地址（IP：端口）,
 * “AddRelPosithBoin”——以相同格式的其他“对”主机的地址
 */
extern int dag_netdb_init(const char *our_host_str, int npairs, const char **addr_port_pairs);

/*将数据写入数组以传输到另一个主机*/
extern unsigned dag_netdb_send(uint8_t *data, unsigned len);

/*读取另一主机发送的数据*/
extern unsigned dag_netdb_receive(const uint8_t *data, unsigned len);

/* 用主机数据库完成工作 */
extern void dag_netdb_finish(void);

/* 传入连接的阻塞IP及其数量 */
extern uint32_t *g_dag_blocked_ips, *g_xdag_white_ips;
extern int g_dag_n_blocked_ips, g_xdag_n_white_ips;

#ifdef __cplusplus
};
#endif

#endif
