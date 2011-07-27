#include "runcommand.h"

#include <unistd.h>
#include <assert.h>
#include <alloca.h>

int fork_and_run(config_process_t *process) {
    int res = fork();
    assert(res >= 0);
    if(res > 0)
        return res;
    char *argv[process->arguments_len+1];
    char *environ[process->environ_len+1];
    int i = 0;
    CONFIG_STRING_LOOP(item, process->arguments) {
        argv[i++] = item->value;
    }
    argv[i] = NULL;
    i = 0;
    CONFIG_STRING_STRING_LOOP(item, process->environ) {
        char *data = alloca(item->key_len + item->value_len + 2);
        strcpy(data, item->key);
        strcat(data, "=");
        strcat(data, item->value);
        environ[i++] = data;
    }
    environ[i] = NULL;

    res = execve(process->executable_path, argv, environ);
    assert(res >= 0);  // which in turn cannot be true
    return 0;
}

