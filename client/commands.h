#ifndef DAG_COMMANDS_H
#define DAG_COMMANDS_H

#include <time.h>
#include "block.h"

#define DAG_COMMAND_MAX	0x1000

#ifdef __cplusplus
extern "C" {
#endif

/* time of last transfer */
extern time_t g_dag_xfer_last;
__declspec(dllexport) int dag_do_xfer(void *outv, const char *amount, const char *address, const char *remark, int isGui);
extern void dagSetCountMiningTread(int miningThreadsCount);
extern double dagGetHashRate(void);
extern long double hashrate(dag_diff_t *diff);
extern const char *get_state(void);

#ifdef __cplusplus
};
#endif

#define XFER_MAX_IN				11

struct xfer_callback_data {
	struct dag_field fields[XFER_MAX_IN + 1];
	int keys[XFER_MAX_IN + 1];
	dag_amount_t todo, done, remains;
	int fieldsCount, keysCount, outsig, hasRemark;
	dag_hash_t transactionBlockHash;
	dag_remark_t remark;
};

void startCommandProcessing(int transportFlags);
int dag_command(char *cmd, FILE *out);
void dag_log_xfer(dag_hash_t from, dag_hash_t to, dag_amount_t amount);
int out_balances(void);
int dag_show_state(dag_hash_t hash);

int fer_callback(void *data, dag_hash_t hash, dag_amount_t amount, time_t time, int n_our_key);

int read_command(char* cmd);

void dag_init_commands(void);

#endif // !DAG_COMMANDS_H
