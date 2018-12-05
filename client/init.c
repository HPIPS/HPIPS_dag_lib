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
    dag_init_path(argv[0]); //根据第一个输入参数来确定文件的名称，并初始化文件变量

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

	char *filename = dag_filename(argv[0]); //根据输入参数的第一值，初始化文件文件名称

	g_progname = strdup(filename); //根据您文件名称定义工程名称
	g_coinname = strdup(filename); //根据文件名称定义通证名称
	free(filename); // 释放定义的内存

	///*前面主要的工作是采用第一个输入参数定义项目的名称和存储文件的名称

	dag_str_toupper(g_coinname); //转化为大写命名
	dag_str_tolower(g_progname); //转换为小写命名

	//是不是打来GUI，如果不打开，打印版本号，目前第一个DAG版本定义为1.0.0
	if (!isGui) {
		printf("%s client/server, version %s.\n", g_progname, DAG_VERSION);
	}

	g_dag_run = 1; //标志DAG为运行的状态
	dag_show_state(0); //设置DAG显示状态为0

	//解析输入参数的详细信息
	for (int i = 1; i < argc; ++i) {
		//提取挖矿地址pool_arg
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
		
		//获取输入参数的详细信息
		if (ARG_EQUAL(argv[i], "-a", "")) { /* 获取挖矿用户地址miner_address*/
			if (++i < argc) miner_address = argv[i];
		} else if(ARG_EQUAL(argv[i], "-c", "")) { /* 另一个完整节点地址 addrports */
			if (++i < argc && n_addrports < 256)
				addrports[n_addrports++] = argv[i];
		} else if(ARG_EQUAL(argv[i], "-d", "")) { /* 守护进程模式*/
#if !defined(_WIN32) && !defined(_WIN64)
			transport_flags |= XDAG_DAEMON;
#endif
		} else if(ARG_EQUAL(argv[i], "-h", "")) { /* 帮助指令 */
			printUsage(argv[0]); //打印帮助内容
			return 0;
		} else if(ARG_EQUAL(argv[i], "-i", "")) { /* 互动模式 ，进入指令互动模式*/
			return terminal();
		} else if(ARG_EQUAL(argv[i], "-z", "")) { /* 内存形式  */
			if (++i < argc) {
				dag_mem_tempfile_path(argv[i]);
			}
		} else if(ARG_EQUAL(argv[i], "-t", "")) { /* 连接测试网标志位 */
			g_dag_testnet = 1;
			g_block_header_type = DAG_FIELD_HEAD_TEST; //块头在测试网络中具有不同的类型
		} else if(ARG_EQUAL(argv[i], "-m", "")) { /* 挖掘线程数设置 */
			if (++i < argc) {
				sscanf(argv[i], "%d", &mining_threads_count);
				if (mining_threads_count < 0) mining_threads_count = 0;
			}
		} else if(ARG_EQUAL(argv[i], "-p", "")) { /* 公开的公钥和指针*/
			if (++i < argc) {
				is_pool = 1;
				pubaddr = argv[i];
			}
		} else if(ARG_EQUAL(argv[i], "-P", "")) { /* 矿池的设置信息 */
			if (++i < argc) {
				pool_arg = argv[i];
			}
		} else if(ARG_EQUAL(argv[i], "-r", "")) { /* 加载块文件，在运行状态 */
			g_dag_run = 0;
		} else if(ARG_EQUAL(argv[i], "-s", "")) { /* 此节点的地址 */
			if (++i < argc)
				bindto = argv[i];
		} else if(ARG_EQUAL(argv[i], "-v", "")) { /* 日志级别 */
			if (++i < argc && sscanf(argv[i], "%d", &level) == 1) {
				dag_set_log_level(level);
			} else {
				printf("Illevel use of option -v\n");
				return -1;
			}
		} else if(ARG_EQUAL(argv[i], "", "-rpc-enable")) { /*启用JSON-RPC服务*/
			is_rpc = 1;
		} else if(ARG_EQUAL(argv[i], "", "-rpc-port")) { /* 设置JSON-RPC服务端口  */
			if(!(++i < argc && sscanf(argv[i], "%d", &rpc_port) == 1)) {
				printf("rpc port not specified.\n");
				return -1;
			}
		} else if(ARG_EQUAL(argv[i], "", "-threads")) { /* 传输层线程数 */
			if (!(++i < argc && sscanf(argv[i], "%d", &transport_threads) == 1))
				printf("Number of transport threads is not given.\n");
		} else if(ARG_EQUAL(argv[i], "", "-dm")) { /* 禁用挖掘 */
			g_disable_mining = 1;
		} else if(ARG_EQUAL(argv[i], "", "-tag")) { /* 池标签 */
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
		} else if(ARG_EQUAL(argv[i], "", "-disable-refresh")) { /* 禁用自动刷新白名单 */
			g_prevent_auto_refresh = 1;
		} else if(ARG_EQUAL(argv[i], "-l", "")) { /* 余额清单 */
			return out_balances();
		} else {
			printUsage(argv[0]);
			return 0;
		}
	}

	//初始化时间参数
	if(!dag_time_init()) {
		printf("Cannot initialize time module\n");
		return -1;
	}

	//初始化网络参数
	if(!dag_network_init()) {
		printf("Cannot initialize network\n");
		return -1;
	}

	//挖矿，矿池的处理
	if(!is_pool && pool_arg == NULL) {
		if(!dag_pick_pool(g_pool_address)) {
			return -1;
		}
		is_miner = 1;
		pool_arg = g_pool_address;
	}

	//挖矿池验证
	if (is_miner) {
		if (is_pool || bindto || n_addrports || transport_threads > 0) {
			printf("Miner can't be a pool or have directly connected to the dag network.\n");
			return -1;
		}
		transport_threads = 0;
	}
	
	g_dag_pool = is_pool; // 移动到这里以避免数据竞争

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
		g_disable_mining = 0;   // 此选项仅用于矿池
	}

	memset(&g_dag_stats, 0, sizeof(g_dag_stats));
	memset(&g_dag_extstats, 0, sizeof(g_dag_extstats));

	dag_mess("Starting %s, version %s", g_progname, DAG_VERSION);
	dag_mess("Starting synchonization engine...");
	if (dag_sync_init()) return -1;
	dag_mess("Starting dnet transport...");
	printf("Transport module: ");

	//启动文件传输系统
	if (dag_transport_start(transport_flags, transport_threads, bindto, n_addrports, addrports)) return -1;
	
	//dag日志初始化
	if (dag_log_init()) return -1;
	
	//初始化主机设置
	if (!is_miner) {
		dag_mess("Reading hosts database...");
		if (dag_netdb_init(pubaddr, n_addrports, addrports)) return -1;
	}
	dag_mess("Initializing cryptography...");
	//加密系统初始化
	if (dag_crypt_init(1)) return -1;

	//初始化钱包
	dag_mess("Reading wallet...");
	if (dag_wallet_init()) return -1;
	//初始化地址
	dag_mess("Initializing addresses...");
	if (dag_address_init()) return -1;

	//rpc 初始化
	if(is_rpc) {
		dag_mess("Initializing RPC service...");
		if(!!dag_rpc_service_start(rpc_port)) return -1;
	}
	dag_mess("Starting blocks engine...");
	//常规块处理
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

//打印参数说明
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
