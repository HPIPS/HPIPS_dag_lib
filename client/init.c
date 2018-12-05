/* cheatcoin main, T13.654-T14.582 $DVS:time$ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <ctype.h>
#if !defined(_WIN32) && !defined(_WIN64)
#include <signal.h>
#endif
#include "system.h"
#include "address.h"
#include "block.h"
#include "crypt.h"
#include "transport.h"
#include "version.h"
#include "wallet.h"
#include "netdb.h"
#include "init.h"
#include "sync.h"
#include "mining_common.h"
#include "commands.h"
#include "terminal.h"
#include "memory.h"
#include "miner.h"
#include "pool.h"
#include "network.h"
#include "utils/log.h"
#include "utils/utils.h"
#include "json-rpc/rpc_service.h"

char *g_coinname, *g_progname;
#define coinname   g_coinname

#define ARG_EQUAL(a,b,c) strcmp(c, "") == 0 ? strcmp(a, b) == 0 : (strcmp(a, b) == 0 || strcmp(a, c) == 0)

int g_dag_state = DAG_STATE_INIT;
int g_dag_testnet = 0;
int g_is_miner = 0;
static int g_is_pool = 0;
int g_dag_run = 0;
time_t g_dag_xfer_last = 0;
enum dag_field_type g_block_header_type = DAG_FIELD_HEAD;
struct dag_stats g_dag_stats;
struct dag_ext_stats g_dag_extstats;
int g_disable_mining = 0;
char g_pool_address[50] = {0};

int(*g_dag_show_state)(const char *state, const char *balance, const char *address) = 0;

void printUsage(char* appName);

int dag_init(int argc, char **argv, int isGui)
{
    dag_init_path(argv[0]); //���ݵ�һ�����������ȷ���ļ������ƣ�����ʼ���ļ�����

	const char *addrports[256] = {0}, *bindto = 0, *pubaddr = 0, *pool_arg = 0, *miner_address = 0;
	int transport_flags = 0, transport_threads = -1, n_addrports = 0, mining_threads_count = 0,
			is_pool = 0, is_miner = 0, level, is_rpc = 0, rpc_port = 0;
	
	memset(addrports, 0, 256);
	
#if !defined(_WIN32) && !defined(_WIN64)
	signal(SIGHUP, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGWINCH, SIG_IGN);
	signal(SIGINT, SIG_IGN);
	signal(SIGTERM, SIG_IGN);
#endif

	char *filename = dag_filename(argv[0]); //������������ĵ�һֵ����ʼ���ļ��ļ�����

	g_progname = strdup(filename); //�������ļ����ƶ��幤������
	g_coinname = strdup(filename); //�����ļ����ƶ���֤ͨ����
	free(filename); // �ͷŶ�����ڴ�

	///*ǰ����Ҫ�Ĺ����ǲ��õ�һ���������������Ŀ�����ƺʹ洢�ļ�������

	dag_str_toupper(g_coinname); //ת��Ϊ��д����
	dag_str_tolower(g_progname); //ת��ΪСд����

	//�ǲ��Ǵ���GUI��������򿪣���ӡ�汾�ţ�Ŀǰ��һ��DAG�汾����Ϊ1.0.0
	if (!isGui) {
		printf("%s client/server, version %s.\n", g_progname, DAG_VERSION);
	}

	g_dag_run = 1; //��־DAGΪ���е�״̬
	dag_show_state(0); //����DAG��ʾ״̬Ϊ0

	//���������������ϸ��Ϣ
	for (int i = 1; i < argc; ++i) {
		//��ȡ�ڿ��ַpool_arg
		if (argv[i][0] != '-') {
			if ((!argv[i][1] || argv[i][2]) && strchr(argv[i], ':')) {
				is_miner = 1;
				pool_arg = argv[i];
			} else {
				printUsage(argv[0]);
				return 0;
			}
			continue;
		}
		
		//��ȡ�����������ϸ��Ϣ
		if (ARG_EQUAL(argv[i], "-a", "")) { /* ��ȡ�ڿ��û���ַminer_address*/
			if (++i < argc) miner_address = argv[i];
		} else if(ARG_EQUAL(argv[i], "-c", "")) { /* ��һ�������ڵ��ַ addrports */
			if (++i < argc && n_addrports < 256)
				addrports[n_addrports++] = argv[i];
		} else if(ARG_EQUAL(argv[i], "-d", "")) { /* �ػ�����ģʽ*/
#if !defined(_WIN32) && !defined(_WIN64)
			transport_flags |= XDAG_DAEMON;
#endif
		} else if(ARG_EQUAL(argv[i], "-h", "")) { /* ����ָ�� */
			printUsage(argv[0]); //��ӡ��������
			return 0;
		} else if(ARG_EQUAL(argv[i], "-i", "")) { /* ����ģʽ ������ָ���ģʽ*/
			return terminal();
		} else if(ARG_EQUAL(argv[i], "-z", "")) { /* �ڴ���ʽ  */
			if (++i < argc) {
				dag_mem_tempfile_path(argv[i]);
			}
		} else if(ARG_EQUAL(argv[i], "-t", "")) { /* ���Ӳ�������־λ */
			g_dag_testnet = 1;
			g_block_header_type = DAG_FIELD_HEAD_TEST; //��ͷ�ڲ��������о��в�ͬ������
		} else if(ARG_EQUAL(argv[i], "-m", "")) { /* �ھ��߳������� */
			if (++i < argc) {
				sscanf(argv[i], "%d", &mining_threads_count);
				if (mining_threads_count < 0) mining_threads_count = 0;
			}
		} else if(ARG_EQUAL(argv[i], "-p", "")) { /* �����Ĺ�Կ��ָ��*/
			if (++i < argc) {
				is_pool = 1;
				pubaddr = argv[i];
			}
		} else if(ARG_EQUAL(argv[i], "-P", "")) { /* ��ص�������Ϣ */
			if (++i < argc) {
				pool_arg = argv[i];
			}
		} else if(ARG_EQUAL(argv[i], "-r", "")) { /* ���ؿ��ļ���������״̬ */
			g_dag_run = 0;
		} else if(ARG_EQUAL(argv[i], "-s", "")) { /* �˽ڵ�ĵ�ַ */
			if (++i < argc)
				bindto = argv[i];
		} else if(ARG_EQUAL(argv[i], "-v", "")) { /* ��־���� */
			if (++i < argc && sscanf(argv[i], "%d", &level) == 1) {
				dag_set_log_level(level);
			} else {
				printf("Illevel use of option -v\n");
				return -1;
			}
		} else if(ARG_EQUAL(argv[i], "", "-rpc-enable")) { /*����JSON-RPC����*/
			is_rpc = 1;
		} else if(ARG_EQUAL(argv[i], "", "-rpc-port")) { /* ����JSON-RPC����˿�  */
			if(!(++i < argc && sscanf(argv[i], "%d", &rpc_port) == 1)) {
				printf("rpc port not specified.\n");
				return -1;
			}
		} else if(ARG_EQUAL(argv[i], "", "-threads")) { /* ������߳��� */
			if (!(++i < argc && sscanf(argv[i], "%d", &transport_threads) == 1))
				printf("Number of transport threads is not given.\n");
		} else if(ARG_EQUAL(argv[i], "", "-dm")) { /* �����ھ� */
			g_disable_mining = 1;
		} else if(ARG_EQUAL(argv[i], "", "-tag")) { /* �ر�ǩ */
			if(i+1 < argc) {
				if(validate_remark(argv[i+1])) {
					memcpy(g_pool_tag, argv[i+1], strlen(argv[i+1]));
					g_pool_has_tag = 1;
					++i;
				} else {
					printf("Pool tag exceeds 32 chars or is invalid ascii.\n");
					return -1;
				}
			} else {
				printUsage(argv[0]);
				return -1;
			}
		} else if(ARG_EQUAL(argv[i], "", "-disable-refresh")) { /* �����Զ�ˢ�°����� */
			g_prevent_auto_refresh = 1;
		} else if(ARG_EQUAL(argv[i], "-l", "")) { /* ����嵥 */
			return out_balances();
		} else {
			printUsage(argv[0]);
			return 0;
		}
	}

	//��ʼ��ʱ�����
	if(!dag_time_init()) {
		printf("Cannot initialize time module\n");
		return -1;
	}

	//��ʼ���������
	if(!dag_network_init()) {
		printf("Cannot initialize network\n");
		return -1;
	}

	//�ڿ󣬿�صĴ���
	if(!is_pool && pool_arg == NULL) {
		if(!dag_pick_pool(g_pool_address)) {
			return -1;
		}
		is_miner = 1;
		pool_arg = g_pool_address;
	}

	//�ڿ����֤
	if (is_miner) {
		if (is_pool || bindto || n_addrports || transport_threads > 0) {
			printf("Miner can't be a pool or have directly connected to the dag network.\n");
			return -1;
		}
		transport_threads = 0;
	}
	
	g_dag_pool = is_pool; // �ƶ��������Ա������ݾ���

	g_is_miner = is_miner;
	g_is_pool = is_pool;

	if (pubaddr && !bindto) {
		char str[64] = {0}, *p = strchr(pubaddr, ':');
		if (p) {
			sprintf(str, "0.0.0.0%s", p);
			bindto = strdup(str);
		}
	}

	if(g_disable_mining && g_is_miner) {
		g_disable_mining = 0;   // ��ѡ������ڿ��
	}

	memset(&g_dag_stats, 0, sizeof(g_dag_stats));
	memset(&g_dag_extstats, 0, sizeof(g_dag_extstats));

	dag_mess("Starting %s, version %s", g_progname, DAG_VERSION);
	dag_mess("Starting synchonization engine...");
	if (dag_sync_init()) return -1;
	dag_mess("Starting dnet transport...");
	printf("Transport module: ");

	//�����ļ�����ϵͳ
	if (dag_transport_start(transport_flags, transport_threads, bindto, n_addrports, addrports)) return -1;
	
	//dag��־��ʼ��
	if (dag_log_init()) return -1;
	
	//��ʼ����������
	if (!is_miner) {
		dag_mess("Reading hosts database...");
		if (dag_netdb_init(pubaddr, n_addrports, addrports)) return -1;
	}
	dag_mess("Initializing cryptography...");
	//����ϵͳ��ʼ��
	if (dag_crypt_init(1)) return -1;

	//��ʼ��Ǯ��
	dag_mess("Reading wallet...");
	if (dag_wallet_init()) return -1;
	//��ʼ����ַ
	dag_mess("Initializing addresses...");
	if (dag_address_init()) return -1;

	//rpc ��ʼ��
	if(is_rpc) {
		dag_mess("Initializing RPC service...");
		if(!!dag_rpc_service_start(rpc_port)) return -1;
	}
	dag_mess("Starting blocks engine...");
	//����鴦��
	if (dag_blocks_start(g_is_pool, mining_threads_count, !!miner_address)) return -1;

	if(!g_disable_mining) {
		dag_mess("Starting pool engine...");
		if(dag_initialize_mining(pool_arg, miner_address)) return -1;
	}

	if (!isGui) {
		if (is_pool || (transport_flags & DAG_DAEMON) > 0) {
			dag_mess("Starting terminal server...");
			pthread_t th;
			const int err = pthread_create(&th, 0, &terminal_thread, 0);
			if(err != 0) {
				printf("create terminal_thread failed, error : %s\n", strerror(err));
				return -1;
			}
		}

		startCommandProcessing(transport_flags);
	}

	return 0;
}

int dag_set_password_callback(int(*callback)(const char *prompt, char *buf, unsigned size))
{
    return dag_user_crypt_action((uint32_t *)(void *)callback, 0, 0, 6);
}

//��ӡ����˵��
void printUsage(char* appName)
{
	printf("Usage: %s flags [pool_ip:port]\n"
		"If pool_ip:port argument is given, then the node operates as a miner.\n"
		"Flags:\n"
		"  -a address     - specify your address to use in the miner\n"
		"  -c ip:port     - address of another xdag full node to connect\n"
		"  -d             - run as daemon (default is interactive mode)\n"
		"  -h             - print this help\n"
		"  -i             - run as interactive terminal for daemon running in this folder\n"
		"  -l             - output non zero balances of all accounts\n"
		"  -m N           - use N CPU mining threads (default is 0)\n"
		"  -p ip:port     - public address of this node\n"
		"  -P ip:port:CFG - run the pool, bind to ip:port, CFG is miners:maxip:maxconn:fee:reward:direct:fund\n"
		"                     miners - maximum allowed number of miners,\n"
		"                     maxip - maximum allowed number of miners connected from single ip,\n"
		"                     maxconn - maximum allowed number of miners with the same address,\n"
		"                     fee - pool fee in percent,\n"
		"                     reward - reward to miner who got a block in percent,\n"
		"                     direct - reward to miners participated in earned block in percent,\n"
		"                     fund - community fund fee in percent\n"
		"  -r             - load local blocks and wait for 'run' command to continue\n"
		"  -s ip:port     - address of this node to bind to\n"
		"  -t             - connect to test net (default is main net)\n"
		"  -v N           - set loglevel to N\n"
		"  -z <path>      - path to temp-file folder\n"
		"  -z RAM         - use RAM instead of temp-files\n"
		"  -rpc-enable    - enable JSON-RPC service\n"
		"  -rpc-port      - set HTTP JSON-RPC port (default is 7667)\n"
		"  -threads N     - create N transport layer threads for pool (default is 6)\n"
		"  -dm            - disable mining on pool (-P option is ignored)\n"
		"  -tag           - tag for pool to distingush pools. Max length is 32 chars\n"
		, appName);
}
