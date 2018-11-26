#ifndef XDAG_NETWORK_H
#define XDAG_NETWORK_H

int dag_network_init(void);
int dag_connect_pool(const char* pool_address, const char** error_message);
void dag_connection_close(int socket);

#endif // XDAG_NETWORK_H
