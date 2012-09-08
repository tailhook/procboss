#define _GNU_SOURCE
#include <sys/prctl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

static void _bossname_init() __attribute__((constructor));
static void _bossname_init() {
    char *keep_vars = getenv("BOSSNAME_KEEPVARS");
    int keep = keep_vars && *keep_vars;
    char *entry = getenv("BOSS_CHILD");
    if(entry) {
        int dlen = strlen(entry);
        char cfgpath[dlen+1];
        char pname[dlen+1];
        int bosspid;
        int childpid;
        int instance_index = -1;
        if(sscanf(entry, "%[^,],%d,%[^,],%d,%d",
                  cfgpath, &bosspid, pname, &childpid, &instance_index) >= 4
           && childpid == getpid()) {
            char *name = getenv("BOSSNAME_OVERRIDE");
            if(!name)
                name = pname;
            prctl(PR_SET_NAME, name);
        }
    } else {
        char *name = getenv("BOSSNAME_OVERRIDE");
        if(name)
            prctl(PR_SET_NAME, name);
    }
    if(keep)
        return;

    // Environment cleanup
    unsetenv("BOSS_CHILD");
    unsetenv("BOSSNAME_OVERRIDE");
    char *preload = getenv("LD_PRELOAD");
    if(preload) {
        char *start = preload;
        while(1) {
            char *colon = strchrnul(start, ':');
            char *slash = colon;
            for(;slash > start && *slash != '/'; --slash);
            if(*slash == '/') ++slash;
            if(colon - slash >= 14 && !strncmp("libbossname.so", slash, 14)) {
                // need to collapse this entry
                if(start == preload && !*colon) {  // only single entry
                    unsetenv("LD_PRELOAD");
                } else {
                    char newpreload[strlen(preload)];
                    memcpy(newpreload, preload, start - preload);
                    newpreload[start - preload] = 0;
                    if(*colon) {
                        strcat(newpreload, colon+1);
                    }
                    setenv("LD_PRELOAD", newpreload, 1);
                }
                break;
            }
            if(!*colon)  // last entry
                break;
            start = colon + 1;
        }
    }
}
