#ifndef _H_SHELLUTIL
#define _H_SHELLUTIL

#include "control.h"

void init_completion(command_def_t *tbl);
int check_command(char **argv, int argc, command_def_t *completion_commands);

#endif // _H_SHELLUTIL
