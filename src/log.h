#ifndef _H_LOG
#define _H_LOG

#include <time.h>
#include "config.h"

#define LCRASH(...) logmsg(LOG_CRASH, ##__VA_ARGS__);
#define LCRITICAL(...) logmsg(LOG_CRITICAL, ##__VA_ARGS__);
#define LRECOVER(...) logmsg(LOG_RECOVER, ##__VA_ARGS__);
#define LWARNING(...) logmsg(LOG_WARNING, ##__VA_ARGS__);
#define STDWARN(...) logstd(LOG_WARNING, ##__VA_ARGS__);
#define LDEAD(...) logmsg(LOG_DEAD, ##__VA_ARGS__);
#define LSTARTUP(...) logmsg(LOG_STARTUP, ##__VA_ARGS__);
#define STDASSERT(value) if(value < 0) { \
    logstd(LOG_CRITICAL, #value); abort(); }
#define STDASSERT2(value, descr) if(value < 0) { \
    logstd(LOG_CRITICAL, #descr); abort(); }

typedef enum {
    LOG_CRASH,
    LOG_CRITICAL,
    LOG_RECOVER,
    LOG_WARNING,
    LOG_DEAD,
    LOG_STARTUP,
} loglevel_t;


void logmsg(int level, char *msg, ...);
void logstd(int level, char *msg, ...);
void openlogs();
void reopenlogs();

#endif
