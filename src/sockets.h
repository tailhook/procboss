#ifndef _H_SOCKETS
#define _H_SOCKETS

#include "config.h"

void open_sockets(bool do_recover);
int open_tcp(config_file_t *file, bool do_listen);
int open_unix(config_file_t *file, bool do_listen);
void close_all(config_process_t *process);

#endif //_H_SOCKETS
