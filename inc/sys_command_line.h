/**
  ******************************************************************************
  * @file:      sys_command_line.h
  * @author:    Cat
  * @version:   V1.0
  * @date:      2018-1-18
  * @brief:     command line
  * @attention:
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
#define CLI_NAME			"vitapatch_bioz"	/* Device name shown in the shell */
#define MAX_COMMAND_NB		32
#define MAX_ARGC			8
#define MAX_LINE_LEN 		80
//#define CLI_PASSWORD		""					/* un-comment to set a password. "" correspond to empty password (just press enter to display shell) */

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

#define LOG(fmt, ...)  do {                                             \
                            printf(CLI_FONT_CYAN						\
								"[Log]: "fmt""							\
								CLI_FONT_DEFAULT"\n",					\
								##__VA_ARGS__);  						\
                        } while(0)

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

#define PRINT_CLI_NAME()	do { printf(CLI_FONT_DEFAULT"\n"CLI_NAME"$ "); } while(0)


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

