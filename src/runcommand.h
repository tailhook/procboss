#ifndef _H_RUNCOMMAND
#define _H_RUNCOMMAND

#include "config.h"

int fork_and_run(config_process_t *process);
void do_run(config_process_t *process, int parentpid, int instance_index);
int do_fork(config_process_t *process, int *instance_index);

#endif // _H_RUNCOMMAND
