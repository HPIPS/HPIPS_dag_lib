/* hosts base, T13.714-T13.785 $DVS:time$ */

#ifndef XDAG_NETDB_H
#define XDAG_NETDB_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ��ֹ�Զ�ˢ�²��� */
extern int g_prevent_auto_refresh;

/* ��ʼ��������������Url HoothStrut��-�����������ⲿ��ַ��IP���˿ڣ�,
 * ��AddRelPosithBoin����������ͬ��ʽ���������ԡ������ĵ�ַ
 */
extern int dag_netdb_init(const char *our_host_str, int npairs, const char **addr_port_pairs);

/*������д�������Դ��䵽��һ������*/
extern unsigned dag_netdb_send(uint8_t *data, unsigned len);

/*��ȡ��һ�������͵�����*/
extern unsigned dag_netdb_receive(const uint8_t *data, unsigned len);

/* ���������ݿ���ɹ��� */
extern void dag_netdb_finish(void);

/* �������ӵ�����IP�������� */
extern uint32_t *g_dag_blocked_ips, *g_xdag_white_ips;
extern int g_dag_n_blocked_ips, g_xdag_n_white_ips;

#ifdef __cplusplus
};
#endif

#endif
