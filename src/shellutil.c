#include "shellutil.h"
#include "linenoise.h"
#include "config.h"

command_def_t *current_completion_table = NULL;

extern config_main_t config;

static void complete_process(const char *data, int offset,
                             linenoiseCompletions *lc) {
    const char *buf = data+offset;
    int blen = strlen(buf);
    CONFIG_STRING_PROCESS_LOOP(item, config.Processes) {
        if(!blen || !strncmp(buf, item->key, blen)) {
            char fullstring[offset + item->key_len + 1];
            memcpy(fullstring, data, offset);
            memcpy(fullstring+offset, item->key, item->key_len+1);
            linenoiseAddCompletion(lc, fullstring);
        }
    }
}

static void completion(const char *buf, linenoiseCompletions *lc) {
    int blen = strlen(buf);
    char *end = strchr(buf, ' ');
    if(end) {
        blen = end - buf;
    }
    for(command_def_t *cur = current_completion_table;
        cur->name; ++cur) {
        if(!blen || !strncmp(buf, cur->name, blen)) {
            if(end) {
                switch(cur->type) {
                case CMD_NOARG:
                    return; // Nothing to complete
                case CMD_PROCMAN:
                    complete_process(buf, strrchr(buf, ' ')+1 - buf, lc);
                    return;
                }
            } else {
                // complete just command
                linenoiseAddCompletion(lc, cur->name);
                return;
            }
        }
    }
}


void init_completion(command_def_t *tbl) {
    current_completion_table = tbl;
    linenoiseSetCompletionCallback(completion);
}
