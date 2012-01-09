#define _BSD_SOURCE
#define _XOPEN_SOURCE
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
#include "bossdcmd.h"
#include "shellutil.h"
#include "proxy.h"

config_main_t config;

void run_shell(int fifofd) {
    char *line;
    char *home = getenv("HOME");
    char filename[home ? strlen(home) + strlen(".bossctl.history")+2 : 0];
    if(home) {
        strcpy(filename, home);
        strcat(filename, "/");
        strcat(filename, ".bossctl.history");
        linenoiseHistoryLoad(filename);
    }

    init_completion(bossd_cmd_table);

    while((line = linenoise("bossd> ")) != NULL) {
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
            if(home) {
                linenoiseHistorySave(filename); /* Save every new entry */
            }
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
    char *config_filename = NULL;
    parse_config(&config, argc, argv, &config_filename);

    if(check_command(argv + optind, argc - optind, bossd_cmd_table)) {
        return 0;
    }

    if(!config.bossd.fifo_len) {
        fprintf(stderr, "No fifo");
        return 1;
    }

    int fd = open(config.bossd.fifo, O_WRONLY);
    if(fd < 0) {
        fprintf(stderr, "Can't open fifo");
        return 1;
    }

    if(argc > optind) {
        if(!strcmp(argv[optind], "show")) {
            int pty = make_pty();
            optind += 1;
            struct iovec vec[(argc-optind)*2+3];
            vec[0].iov_base = "startin ";
            vec[0].iov_len = strlen("startin ");
            vec[1].iov_base = ptsname(pty);
            vec[1].iov_len = strlen(vec[1].iov_base);
            vec[2].iov_base = " ";
            vec[2].iov_len = 1;
            for(int i = 0; i < argc-optind; ++i) {
                vec[i*2+3].iov_base = argv[optind + i];
                vec[i*2+3].iov_len = strlen(argv[optind + i]);
                if(i == argc-optind-1) {
                    vec[i*2+4].iov_base = "\n";
                    vec[i*2+4].iov_len = 1;
                } else {
                    vec[i*2+4].iov_base = " ";
                    vec[i*2+4].iov_len = 1;
                }
            }
            writev(fd, vec, sizeof(vec)/sizeof(vec[0]));
            start_proxy(pty);
        } else {
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
        }
    } else {
        run_shell(fd);
    }

    close(fd);

    if(config.bossctl.show_tree) {
        execlp("bosstree", "bosstree",
            config.bossctl.bosstree_options,
            "-c", config_filename,
            NULL);
    } else if(config.bossctl.show_tail) {
        execlp(
            config.bossctl.tail_command,
            config.bossctl.tail_command,
            config.bossctl.tail_options,
            config.bossd.logging.file,
            NULL);
    }

    config_free(&config);
    return 0;
}
