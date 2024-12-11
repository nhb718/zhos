/**
 * 手写操作系统
 *
 * 文件名: log.h
 * 功  能: 日志输出
 */

#ifndef LOG_H
#define LOG_H


void log_init(void);
void log_printf(const char * fmt, ...);


#endif // LOG_H
