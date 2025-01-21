#pragma once

#include <iostream>
#include <string>
#include <map>
#include <cstdio>
#include <cstring>
#include <time.h>
#include <stdarg.h>
#include <unistd.h>

static const char *filename = "server.log";

// 日志等级
enum loglevel_t
{
    TRACE,
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    FATAL
};
static std::map<loglevel_t, std::string> ltos = {{TRACE, "TRACE"}, {DEBUG, "DEBUG"}, {INFO, "INFO"}, {WARNING, "WARNING"}, {ERROR, "ERROR"}, {FATAL, "FATAL"}};

// 日志格式：log = title(log level, time, pid) + body
void LogMessage(loglevel_t lv, const char *format, ...)
{
    // 1. title
    time_t t = time(nullptr);
    struct tm *tp = localtime(&t);
    // 2. time = y-m-d h:m:s
    char timestr[64];
    memset(timestr, 0, sizeof(timestr));
    snprintf(timestr, sizeof(timestr), "%d-%d-%d %d:%d:%d", tp->tm_year + 1900, tp->tm_mon + 1, tp->tm_mday, tp->tm_hour, tp->tm_min, tp->tm_sec);
    std::string logtitle = ltos[lv] + " " + timestr + " " + std::to_string(getpid());

    // 3. body
    va_list ap;
    char logbody[128];
    memset(logbody, 0, sizeof(logbody));
    va_start(ap, format);
    vsnprintf(logbody, sizeof(logbody), format, ap);
    va_end(ap);
	
    // 4. 输出
    
    // 输出到终端
    // combine and output
    printf("[%s] %s", logtitle.c_str(), logbody);

    // 保存到文件
    // FILE *fp = fopen(filename, "a");

    // fprintf(fp, "[%s] %s", logtitle.c_str(), logbody);

    // fclose(fp);
}

// LogMessage(FATAL, "epoll create failed, errno: %d - strerror: %s\n", errno, strerror(errno));