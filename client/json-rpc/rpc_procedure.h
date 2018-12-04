//
//  rpc_procedure.h
//  xdag
//
//  Created by Rui Xie on 6/24/18.
//  Copyright © 2018 xrdavies. All rights reserved.
//

#ifndef XDAG_RPC_PROCEDURE_H
#define XDAG_RPC_PROCEDURE_H

#include "cJSON.h"
#include "cJSON_Utils.h"
#include "rpc_procedures.h"

struct dag_rpc_context{
	void *data;
	int error_code;
	char * error_message;
	char rpc_version[8];
} ;

typedef cJSON* (*dag_rpc_function)(struct dag_rpc_context *context, cJSON *params, cJSON *id, char *version);

struct dag_rpc_procedure {
	char * name;
	dag_rpc_function function;
	void *data;
};

#ifdef __cplusplus
extern "C" {
#endif
/* 登记程序 */
extern int dag_rpc_service_register_procedure(dag_rpc_function function_pointer, char *name, void *data);

/* 注销程序 */
extern int dag_rpc_service_unregister_procedure(char *name);

/* 列表登记程序 */
extern int dag_rpc_service_list_procedures(char *);

/* 取消所有程序的注册 */
extern int dag_rpc_service_clear_procedures(void);

/* 处理RPC请求 */
extern cJSON *dag_rpc_handle_request(char* buffer);

#ifdef __cplusplus
};
#endif
		
#endif //XDAG_RPC_PROCEDURE_H
