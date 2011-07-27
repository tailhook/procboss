#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>

#include "config.h"
#include "runcommand.h"

config_main_t config;

int main(int argc, char **argv) {
    config_load(&config, argc, argv);
    CONFIG_STRING_PROCESS_LOOP(item, config.Processes) {
        int pid = fork_and_run(&item->value);
        printf("RUN %.*s with name %d\n", (int)item->key_len, item->key, pid);
    }
    int ret;
    while(1) {
        int res = wait(&ret);
        if(res == ECHILD) break;
        assert(res != EINVAL);
    }
    config_free(&config);
    return 0;
}
