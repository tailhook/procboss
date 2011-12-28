#include <ctype.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

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
                case CMD_INSTMAN:
                case CMD_GROUPMAN:
                    complete_process(buf, strrchr(buf, ' ')+1 - buf, lc);
                    return;
                case CMD_INSTMAN1:
                case CMD_GROUPMAN1:
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

int check_command(char **argv, int argc, command_def_t *completion_commands) {
    if(!argc) return 0;
    if(!strcmp(argv[0], "compword")) {
        if(argc == 1) {  // completing all commands
            for(command_def_t *val = completion_commands;
                val->name; ++val) {
                printf("%s\n", val->name);
            }
        } else if(argc == 2) {
            int len = strlen(argv[1]);
            for(command_def_t *val = completion_commands;
                val->name; ++val) {
                if(!len || !strncmp(argv[1], val->name, len)) {
                    printf("%s\n", val->name);
                }
            }
        } else {  // completing process
            for(command_def_t *val = completion_commands;
                val->name; ++val) {
                if(!strcmp(argv[1], val->name)) {
                    if(val->type == CMD_NOARG) return 1;
                    break;
                }
            }
            int len = strlen(argv[argc-1]);
            CONFIG_STRING_PROCESS_LOOP(item, config.Processes) {
                if(!len || !strncmp(argv[argc-1], item->key, len)) {
                    printf("%s\n", item->key);
                }
                CONFIG_STRING_LOOP(tag, item->value.tags) {
                    if(!len || !strncmp(argv[argc-1], tag->value, len)) {
                        printf("%s\n", tag->value);
                    }
                }
            }
        }
        return 1;
    }
    if(!strcmp(argv[0], "compdescr")) {
        if(argc == 1) {  // completing all commands
            for(command_def_t *val = completion_commands;
                val->name; ++val) {
                printf("%s %s\n", val->name, val->description);
            }
        } else if(argc == 2) {
            int len = strlen(argv[1]);
            for(command_def_t *val = completion_commands;
                val->name; ++val) {
                if(!len || !strncmp(argv[1], val->name, len)) {
                    printf("%s %s\n", val->name, val->description);
                }
            }
        } else {  // completing process
            for(command_def_t *val = completion_commands;
                val->name; ++val) {
                if(!strcmp(argv[1], val->name)) {
                    if(val->type == CMD_NOARG) return 1;
                    break;
                }
            }
            int len = strlen(argv[argc-1]);
            CONFIG_STRING_PROCESS_LOOP(item, config.Processes) {
                if(!len || !strncmp(argv[argc-1], item->key, len)) {
                    printf("%s", item->key);
                    int chars = 0;
                    CONFIG_STRING_LOOP(arg, item->value.arguments) {
                        char *end = arg->value;
                        while(isprint(*end)) ++end;
                        if(end - arg->value + chars >= 70) {
                            printf(" %.*s...", 70 - chars, arg->value);
                            break;
                        } else if(*end) {
                            printf(" %.*s...",
                                (int)(end - arg->value), arg->value);
                            break;
                        } else {
                            printf(" %s", arg->value);
                            chars += arg->value_len;
                        }
                    }
                    printf("\n");
                }
                CONFIG_STRING_LOOP(tag, item->value.tags) {
                    if(!len || !strncmp(argv[argc-1], tag->value, len)) {
                        printf("%s tagged processes\n", tag->value);
                    }
                }
            }
        }
        return 1;
    }
    return 0;
}

void parse_config(config_main_t *cfg, int argc, char **argv, char **filename) {
    coyaml_context_t ctx;
    if(config_context(&ctx, cfg) < 0) {
        perror(argv[0]);
        exit(1);
    }
    // sorry, fixing shortcommings of coyaml
    ctx.cmdline->full_description = "";
    if(coyaml_cli_prepare(&ctx, argc, argv)) {
        if(errno == ECOYAML_CLI_HELP) {
            char *fname = strrchr(argv[0], '/');
            if(!fname) {
                fname = argv[0];
            } else {
                fname += 1;
            }
            execlp("man", "man", fname, NULL);
        }
        if(errno > ECOYAML_MAX || errno < ECOYAML_MIN) {
            perror(argv[0]);
        }
        config_free(cfg);
        coyaml_context_free(&ctx);
        exit(1);
    }
    coyaml_readfile_or_exit(&ctx);
    coyaml_env_parse_or_exit(&ctx);
    coyaml_cli_parse_or_exit(&ctx, argc, argv);
    if(filename) {
        *filename = realpath(ctx.root_filename, NULL);
    }
    coyaml_context_free(&ctx);
}
