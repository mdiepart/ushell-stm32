/**
  ******************************************************************************
  * @file:      sys_command_line.h
  * @author:    Cat
  * @author: 	Morgan Diepart
  * @version:   V1.0
  * @date:      2022-08-24
  * @brief:     command line
  *
  ******************************************************************************
  */

#ifndef __SYS_COMMAND_LINE_H
#define __SYS_COMMAND_LINE_H

#include "main.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "sys_queue.h"
#include "vt100.h"

/*
 *  Macro config
 */
#define CLI_ENABLE          true            	/* command line enable/disable */
#define HISTORY_MAX         10                  /* maximum number of history command */
#define MAX_COMMAND_NB		32
#define MAX_ARGC			8
#define MAX_LINE_LEN 		80

#ifndef CLI_DISABLE
    #define CLI_INIT(...)       cli_init(__VA_ARGS__)
    #define CLI_RUN(...)        cli_run(__VA_ARGS__)
	#define CLI_ADD_CMD(...)	cli_add_command(__VA_ARGS__)
#else
    #define CLI_INIT(...)       ;
    #define CLI_RUN(...)        ;
	#define CLI_ADD_CMD(...)	;
#endif /* CLI_DISABLE */

#define ERR(fmt, ...)  do {                                             \
                            fprintf(stderr,								\
								CLI_FONT_RED							\
								"[ERROR] %s(%d): "fmt""					\
								CLI_FONT_DEFAULT"\n",  					\
                                __FUNCTION__, __LINE__, ##__VA_ARGS__); \
                        }while(0)

#define LOG(LOG_CAT, fmt, ...)  										\
						if((1<<LOG_CAT)&cli_log_stat) {                     \
                            printf(CLI_FONT_CYAN						\
								"[%s]: "fmt""							\
								CLI_FONT_DEFAULT"\n",					\
								cli_logs_names[LOG_CAT],					\
								##__VA_ARGS__);  						\
                        }

#define DBG(fmt, ...)  do {                                             \
                            printf(CLI_FONT_YELLOW						\
							"[Debug] %s(%d): "fmt""						\
							CLI_FONT_DEFAULT"\n",						\
                                __FUNCTION__, __LINE__, ##__VA_ARGS__); \
                        } while(0)

#define DIE(fmt, ...)   do {                                       		\
                            TERMINAL_FONT_RED();                        \
                            TERMINAL_HIGHLIGHT();                       \
                            fprintf(stderr, 							\
								"### DIE ### %s(%d): "fmt"\n",     		\
                                __FUNCTION__, __LINE__, ##__VA_ARGS__); \
                        } while(1) /* infinite loop */

#define NL1()           do { printf("\n"); } while(0)
#define NL2()           do { printf("\n\n"); } while(0)
#define NL3()           do { printf("\n\n\n"); } while(0)

#ifdef CLI_NAME
#define PRINT_CLI_NAME()	do { printf(CLI_FONT_DEFAULT"\n"XSTRING(CLI_NAME)"$ "); } while(0)
#else
#define PRINT_CLI_NAME()	do { printf(CLI_FONT_DEFAULT"\n#$ "); } while(0)
#endif
enum cli_log_categories {
	CLI_LOG_SHELL = 0,

#ifdef CLI_ADDITIONAL_LOG_CATEGORIES
#define X(name, b) CLI_LOG_##name,
CLI_ADDITIONAL_LOG_CATEGORIES
#undef X
#endif

	CLI_LAST_LOG_CATEGORY,
};

extern char *cli_logs_names[];

extern uint32_t cli_log_stat;


/**
  * @brief  command line init.
  * @param  handle to uart peripheral
  * @retval null
  */
void 		cli_init(UART_HandleTypeDef *handle_uart);

/**
  * @brief  command line task, schedule by sys. every 50ms
  * @param  null
  * @retval null
  */
void 		cli_run(void);

void 		cli_add_command(const char *command, const char *help, uint8_t (*exec)(int argc, char *argv[]));

#endif /* __SYS_COMMAND_LINE_H */

