/* logging, T13.670-T13.788 $DVS:time$ */

#ifndef XDAG_LOG_H
#define XDAG_LOG_H

enum xdag_debug_levels {
	XDAG_NOERROR,
	XDAG_FATAL,
	XDAG_CRITICAL,
	XDAG_INTERNAL,
	XDAG_ERROR,
	XDAG_WARNING,
	XDAG_MESSAGE,
	XDAG_INFO,
	XDAG_DEBUG,
	XDAG_TRACE,
};

#ifdef __cplusplus
extern "C" {
#endif
	
extern int dag_log(const char *logfile, int level, const char *format, ...);

extern char *dag_log_array(const void *arr, unsigned size);

extern int dag_log_init(void);

#define xdag_log_hash(hash) dag_log_array(hash, sizeof(dag_hash_t))

// sets the maximum error level for output to the log, returns the previous level (0 - do not log anything, 9 - all)
extern int xdag_set_log_level(int level);
	
#ifdef __cplusplus
};
#endif

#define DAG_LOG_FILE "dag.log"
#define DNET_LOG_FILE "dnet.log"

#define dag_fatal(...) dag_log(DAG_LOG_FILE, XDAG_FATAL   , __VA_ARGS__)
#define dag_crit(...)  dag_log(DAG_LOG_FILE, XDAG_CRITICAL, __VA_ARGS__)
#define dag_err(...)   dag_log(DAG_LOG_FILE, XDAG_ERROR   , __VA_ARGS__)
#define dag_warn(...)  dag_log(DAG_LOG_FILE, XDAG_WARNING , __VA_ARGS__)
#define dag_mess(...)  dag_log(DAG_LOG_FILE, XDAG_MESSAGE , __VA_ARGS__)
#define dag_info(...)  dag_log(DAG_LOG_FILE, XDAG_INFO    , __VA_ARGS__)
#ifndef NDEBUG
#define dag_debug(...) dag_log(DAG_LOG_FILE, XDAG_DEBUG   , __VA_ARGS__)
#else
#define xdag_debug(...)
#endif

#define dnet_fatal(...) dag_log(DNET_LOG_FILE, XDAG_FATAL   , __VA_ARGS__)
#define dnet_crit(...)  dag_log(DNET_LOG_FILE, XDAG_CRITICAL, __VA_ARGS__)
#define dnet_err(...)   dag_log(DNET_LOG_FILE, XDAG_ERROR   , __VA_ARGS__)
#define dnet_warn(...)  dag_log(DNET_LOG_FILE, XDAG_WARNING , __VA_ARGS__)
#define dnet_mess(...)  dag_log(DNET_LOG_FILE, XDAG_MESSAGE , __VA_ARGS__)
#define dnet_info(...)  dag_log(DNET_LOG_FILE, XDAG_INFO    , __VA_ARGS__)
#ifndef NDEBUG
#define dnet_debug(...) dag_log(DNET_LOG_FILE, XDAG_DEBUG   , __VA_ARGS__)
#else
#define dnet_debug(...)
#endif

#endif
