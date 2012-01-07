#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <getopt.h>
#include <errno.h>

#include "linenoise.h"
#include "config.h"
#include "bossruncmd.h"
#include "shellutil.h"

char *config_filenames[] = {
    "./bossrun.yaml",
    "./boss.yaml",
    "./config/bossrun.yaml",
    "./config/boss.yaml",
    NULL
    };

config_main_t config;

void run_shell(int fifofd) {
    char *line;

    init_completion(bossrun_cmd_table);
    linenoiseHistoryLoad(".bossrc.history");

    while((line = linenoise("bossrun> ")) != NULL) {
        if (line[0] != '\0') {
            struct iovec vec[2] = {
                {iov_base: line,
                 iov_len: strlen(line)},
                {iov_base: "\n",
                 iov_len: 1}
                 };

            int res = writev(fifofd, vec, 2);
            if(!res) {
                fprintf(stderr, "Can't write to fifo");
                fflush(stderr);
                abort();
            }
            linenoiseHistoryAdd(line);
            linenoiseHistorySave(".bossrc.history"); /* Save every new entry */
        }
        free(line);
    }
}

void parse_config(config_main_t *cfg, int argc, char **argv, char **filename) {
    coyaml_context_t ctx;
    if(config_context(&ctx, cfg) < 0) {
        perror(argv[0]);
        exit(1);
    }

    if(!getenv("BOSS_CONFIG")) {
        for(char **fn = &config_filenames[0]; *fn; ++fn) {
            if(!access(*fn, F_OK)) {
                ctx.root_filename = *fn;
                break;
            }
        }
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

int main(int argc, char *argv[]) {
    char *config_filename;
    parse_config(&config, argc, argv, &config_filename);
    if(!config.bossrun.fifo_len) {
        fprintf(stderr, "No fifo");
        return 1;
    }

    if(check_command(argv + optind, argc - optind, bossrun_cmd_table)) {
        return 0;
    }

    int fd = open(config.bossrun.fifo, O_WRONLY);
    if(fd < 0) {
        fprintf(stderr, "Can't open fifo");
        return 1;
    }

    if(argc > optind) {
        struct iovec vec[(argc-optind)*2];
        for(int i = 0; i < argc-optind; ++i) {
            vec[i*2].iov_base = argv[optind + i];
            vec[i*2].iov_len = strlen(argv[optind + i]);
            if(i == argc-optind-1) {
                vec[i*2+1].iov_base = "\n";
                vec[i*2+1].iov_len = 1;
            } else {
                vec[i*2+1].iov_base = " ";
                vec[i*2+1].iov_len = 1;
            }
        }
        writev(fd, vec, sizeof(vec)/sizeof(vec[0]));
    } else {
        run_shell(fd);
    }

    close(fd);

    if(config.bossctl.show_tree) {
        execlp("bosstree", "bosstree",
            config.bossctl.bosstree_options,
            "-c", config_filename,
            NULL);
    }

    config_free(&config);
    return 0;
}
