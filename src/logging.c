#ifndef LOGGING_C
#define LOGGING_C

#include <stdlib.h>

#ifndef LOG_LEVEL
#define LOG_LEVEL 3
#endif

#define ABORT(fmt, ...)   do{log_printf("Abort", __FILE__, __LINE__, fmt, ## __VA_ARGS__); exit(-1);}while(0)
#define ASSERT(cond, fmt, ...) do{if(!(cond)) {log_printf("Assert", __FILE__, __LINE__, fmt, ## __VA_ARGS__); exit(-3);}}while(0)


#if LOG_LEVEL >= 1
#define ERROR(fmt, ...) do{log_printf("Error", __FILE__, __LINE__, fmt, ## __VA_ARGS__); exit(-2);}while(0)
#define DBG_ASSERT(cond, fmt, ...) do{if(!(cond)) {log_printf("Assert", __FILE__, __LINE__, fmt, ## __VA_ARGS__); exit(-3);}}while(0)
#else
#define ERROR(fmt, ...) do{}while(0)
#define DBG_ASSERT(cond, fmt, ...) do{}while(0)
#endif


#if LOG_LEVEL >= 2
#define WARN(fmt, ...) log_printf("Warn", __FILE__, __LINE__, fmt, ## __VA_ARGS__)
#else
#define WARN(fmt, ...) do{}while(0)
#endif

#if LOG_LEVEL >= 3
#define INFO(fmt, ...) log_printf("Info", __FILE__, __LINE__, fmt, ## __VA_ARGS__)
#else
#define INFO(fmt, ...) do{}while(0)
#endif


#if LOG_LEVEL >= 4
#define XINFO(fmt, ...) log_printf("XInfo", __FILE__, __LINE__, fmt, ## __VA_ARGS__)
#else
#define XINFO(fmt, ...) do{}while(0)
#endif



void log_printf(char*level, char *file, int line, const char *fmt, ...);


#if __INCLUDE_LEVEL__ == 0 || defined(NOLIB)

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

void log_printf(char *level, char *file, int line, const char *fmt, ...)
{
	char fn[20+1] = {0};
	int i = strlen(file)-1;
	int j = 19;
	int slash_found = 0;
	while (j >= 0 && i >=0 && (file[i] != '/' || !slash_found)) {
		slash_found |= (file[i] == '/');
		fn[j--] = file[i--];
	}
	
	fprintf(stderr, "%6s:%20s:%4d: ",level, &fn[j+1], line);
	va_list args;
	va_start (args, fmt);
	vfprintf (stderr, fmt, args);	
	va_end (args);
	fprintf(stderr, "\n");
	fflush(stderr);
}

#endif
#endif
