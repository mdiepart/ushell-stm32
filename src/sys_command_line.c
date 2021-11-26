/**
  ******************************************************************************
  * @file:      sys_command_line.c
  * @author:    Cat
  * @version:   V1.0
  * @date:      2018-1-18
  * @brief:     command line
  * @attention:
  * 
  *             V0.5: [add] colorful log print API
  *             V0.6: [add] history command support
  * 
  ******************************************************************************
  */

#include "main.h"
#include <stdbool.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include "../inc/sys_command_line.h"

#define RX_BUFF_TYPE        QUEUE64_S

/*******************************************************************************
 *
 * 	Typedefs
 *
 ******************************************************************************/


/*
 * Buffer for current line
 */
typedef struct {
    uint8_t buff[MAX_LINE_LEN];
    uint8_t len;
} HANDLE_TYPE_S;

/*
 * Command entry
 */
typedef struct {
    const char *pCmd;
    const char *pHelp;
    uint8_t (*pFun)(int argc, char *argv[]);
} COMMAND_S;

/*
 * Command line history
 */
typedef struct {
    char cmd[HISTORY_MAX][MAX_LINE_LEN];
    uint8_t count;
    uint8_t latest;
    uint8_t show;
}HISTORY_S;

/*******************************************************************************
 *
 * 	Internal variables
 *
 ******************************************************************************/

RX_BUFF_TYPE        	cli_rx_buff;
unsigned char 			cBuffer;
RX_BUFF_TYPE 			cli_rx_buff; 				/* 64 bytes FIFO, saving commands from the terminal */
UART_HandleTypeDef 		*huart_shell;
COMMAND_S				CLI_commands[MAX_COMMAND_NB];
static HISTORY_S 		history;

const char 				cli_help_help[] 			= "show commands";
const char 				cli_clear_help[] 			= "clear the screen";
const char 				cli_reboot_help[] 			= "reboot MCU";
bool 					cli_password_ok 			= false;

/*******************************************************************************
 *
 * 	Internal functions declaration
 *
 ******************************************************************************/

static void 	cli_history_add			(char* buff);
static uint8_t 	cli_history_show		(uint8_t mode, char** p_history);
void 			HAL_UART_RxCpltCallback	(UART_HandleTypeDef * huart);
static void 	cli_rx_handle			(RX_BUFF_TYPE *rx_buff);
static void 	cli_tx_handle			(void);
uint8_t 		cli_help				(int argc, char *argv[]);
uint8_t 		cli_clear				(int argc, char *argv[]);
uint8_t 		cli_reboot				(int argc, char *argv[]);
void 			cli_add_command			(const char *command, const char *help, uint8_t (*exec)(int argc, char *argv[]));
void 			greet					(void);


/*******************************************************************************
 *
 * 	These functions need to be redefined over the [_weak] versions defined by
 * 	GCC (or in syscalls.c by cubeMX) to make the stdio library functional.
 *
 ******************************************************************************/

int _write(int file, char *data, int len){
	if(file != STDOUT_FILENO && file != STDERR_FILENO){
		errno = EBADF;
		return -1;
	}
	HAL_StatusTypeDef status = HAL_UART_Transmit(huart_shell, (uint8_t *)data, len, 100);

	if(status == HAL_OK){
		return len;
	}else{
		return 0;
	}
}

__attribute__((weak)) int _isatty(int file){
	switch(file){
	case STDERR_FILENO:
	case STDIN_FILENO:
	case STDOUT_FILENO:
		return 1;
	default:
		errno = EBADF;
		return 0;
	}
}

/*******************************************************************************
 *
 * 	Functions definitions
 *
 ******************************************************************************/

/**
  * @brief          add a command to the history
  * @param  buff:   command
  * @retval         null
  */
static void cli_history_add(char* buff)
{
    uint16_t len;
    uint8_t index = history.latest;

    if (NULL == buff) return;

    len = strlen((const char *)buff);
    if (len >= MAX_LINE_LEN) return;  /* command too long */

    /* find the latest one */
    if (0 != index) {
        index--;
    } else {
        index = HISTORY_MAX - 1;
    }

    if (0 != memcmp(history.cmd[index], buff, len)) {
        /* if the new one is different with the latest one, the save */
        memset((void *)history.cmd[history.latest], 0x00, MAX_LINE_LEN);
        memcpy((void *)history.cmd[history.latest], (const void *)buff, len);
        if (history.count < HISTORY_MAX) {
            history.count++;
        }

        history.latest++;
        if (history.latest >= HISTORY_MAX) {
            history.latest = 0;
        }
    }

    history.show = 0;
}


/**
  * @brief              returns a command from the history
  * @param  mode:       TRUE for look up, FALSE for look down
  * @param  p_history:  target history command
  * @retval             TRUE for no history found, FALSE for success
  */
static uint8_t cli_history_show(uint8_t mode, char** p_history)
{
    uint8_t err = true;
    uint8_t num;
    uint8_t index;

    if (0 == history.count) return err;

    if (true == mode) {
        /* look up */
        if (history.show < history.count) {
            history.show++;
        }
    } else {
        /* look down */
        if (1 < history.show) {
            history.show--;
        }
    }

    num = history.show;
    index = history.latest;
    while (num) {
        if (0 != index) {
            index--;
        } else {
            index = HISTORY_MAX - 1;
        }
        num--;
    }

    err = false;
    *p_history = history.cmd[index];

    return err;
}

void cli_init(UART_HandleTypeDef *handle_uart)
{
	huart_shell = handle_uart;

    memset((uint8_t *)&cli_rx_buff, 0, sizeof(RX_BUFF_TYPE));
    memset((uint8_t *)&history, 0, sizeof(history));

    HAL_UART_MspInit(huart_shell);
    HAL_UART_Receive_IT(huart_shell, &cBuffer, 1);

    for(size_t j = 0; j < MAX_COMMAND_NB; j++){
    	CLI_commands[j].pCmd = "";
    	CLI_commands[j].pFun = NULL;
    }

    cli_add_command("help", cli_help_help, cli_help);
    cli_add_command("cls", cli_clear_help, cli_clear);
    cli_add_command("reboot", cli_reboot_help, cli_reboot);

#ifndef CLI_PASSWORD
    cli_password_ok = true;
    greet();
#endif


}

/*
 * Callback function for UART IRQ when it is done receivign a char
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef * huart){
	QUEUE_PUT(cli_rx_buff, cBuffer);
	HAL_UART_Receive_IT(huart, &cBuffer, 1);
}


/**
  * @brief  handle commands from the terminal
  * @param  commands
  * @retval null
  */
static void cli_rx_handle(RX_BUFF_TYPE *rx_buff)
{
    static HANDLE_TYPE_S Handle = {.len = 0, .buff = {0}};
    uint8_t i = Handle.len;
    uint8_t cmd_match = false;
    uint8_t exec_req = false;

    /*  ---------------------------------------
        Step1: save chars from the terminal
        ---------------------------------------
     */
    bool newChar = true;
    while(newChar) {
        if(Handle.len < MAX_LINE_LEN) {  /* check the buffer */
        	newChar = QUEUE_GET((*rx_buff), Handle.buff[Handle.len]);

            /* new char coming from the terminal, copy it to Handle.buff */
            if(newChar) {
                /* KEY_BACKSPACE -->get DELETE key from keyboard */
                if (Handle.buff[Handle.len] == KEY_BACKSPACE || Handle.buff[Handle.len] == KEY_DEL) {
                    /* buffer not empty */
                    if (Handle.len > 0) {
                        /* delete a char in terminal */
                        TERMINAL_MOVE_LEFT(1);
                        TERMINAL_CLEAR_END();
                        Handle.buff[Handle.len] = '\0';
                        Handle.len--;
                    }

                } else if(Handle.buff[Handle.len] == KEY_ENTER){
                	exec_req = true;
                	Handle.len++;
                }else if(strstr((const char *)Handle.buff, KEY_DELETE) != NULL){
                	strcpy((char *)&Handle.buff[Handle.len-3], (char *)&Handle.buff[Handle.len+1]);
                	Handle.len -= 3;
            	}else{
                    Handle.len++;
                }

            } else if(cli_password_ok){
                /* all chars copied to Handle.buff */
                uint8_t key = 0;
                uint8_t err = 0xff;
                char *p_hist_cmd = 0;

                if (Handle.len >= 3) {
                    if (strstr((const char *)Handle.buff, KEY_UP) != NULL) {
                        key = 1;
                        TERMINAL_MOVE_LEFT(Handle.len-3);
                        TERMINAL_CLEAR_END();
                        err = cli_history_show(true, &p_hist_cmd);
                    } else if (strstr((const char *)Handle.buff, KEY_DOWN) != NULL) {
                        key = 2;
                        TERMINAL_MOVE_LEFT(Handle.len-3);
                        TERMINAL_CLEAR_END();
                        err = cli_history_show(false, &p_hist_cmd);
                    } else if (strstr((const char *)Handle.buff, KEY_RIGHT) != NULL) {
                        key = 3;
                    } else if (strstr((const char *)Handle.buff, KEY_LEFT) != NULL) {
                        key = 4;
                    }

                    if (key != 0) {
                        if (!err) {
                            memset(&Handle, 0x00, sizeof(Handle));
                            memcpy(Handle.buff, p_hist_cmd, strlen(p_hist_cmd));
                            Handle.len = strlen(p_hist_cmd);
                            Handle.buff[Handle.len] = '\0';
                            printf("%s", Handle.buff);  /* display history command */
                        } else if (err && (0 != key)) {
                            /* no history found */
                            TERMINAL_MOVE_LEFT(Handle.len-3);
                            TERMINAL_CLEAR_END();
                            memset(&Handle, 0x00, sizeof(Handle));
                        }
                    }
                }

                if ((key == 0) && (Handle.len > i)) {
                    /* display char in terminal */
                    for (; i < Handle.len; i++) {
                    	printf("%c", Handle.buff[i]);

                    }
                }
                break;
            }

        } else {
            /* buffer full */
            break;
        } /*end if(Handle.len > HANDLE_LEN) */
    } /* end While(1) */

    /*  ---------------------------------------
        Step2: handle the commands
        ---------------------------------------
     */
    if(exec_req && !cli_password_ok){
#ifdef CLI_PASSWORD
    	Handle.buff[Handle.len-1] = '\0';
    	if(strcmp((char *)Handle.buff, CLI_PASSWORD) == 0){
    		cli_password_ok = true;
    		greet();
    	}
    	Handle.len = 0;
#else
    	cli_password_ok = true;
    	greet();
#endif
    }else if(exec_req && (Handle.len == 1)) {
        /* KEY_ENTER -->ENTER key from terminal */
    	PRINT_CLI_NAME();
        Handle.len = 0;
    } else if(exec_req && Handle.len > 1) {  /* check for the length of command */
		NL1();
		Handle.buff[Handle.len - 1] = '\0';
		cli_history_add((char *)Handle.buff);
		char *command = strtok((char *)Handle.buff, " \t");

		/* looking for a match */
		for(i = 0; i < MAX_COMMAND_NB; i++) {
			if(0 == strcmp(command, CLI_commands[i].pCmd)) {
				cmd_match = true;

				//Split arguments string to argc/argv
				uint8_t argc = 1;
				char 	*argv[MAX_ARGC];
				argv[0] = command;

				char *token = strtok(NULL, " \t");
				while(token != NULL){
					if(argc >= MAX_ARGC){
						PRINTF_COLOR(E_FONT_RED, "Maximum number of arguments is %d. Ignoring the rest of the arguments.\r\n", MAX_ARGC-1);
						break;
					}
					argv[argc] = token;
					argc++;
					token = strtok(NULL, " \t");
				}

				if(CLI_commands[i].pFun != NULL) {
					/* call the func. */
					TERMINAL_HIDE_CURSOR();
					uint8_t result = CLI_commands[i].pFun(argc, argv);

					if(result == EXIT_SUCCESS){
						PRINTF_COLOR(E_FONT_GREEN, "(%s returned %d)\r\n", command, result);
					}else{
						PRINTF_COLOR(E_FONT_RED, "(%s returned %d)\r\n", command, result);
					}
					TERMINAL_SHOW_CURSOR();
					break;
				} else {
					/* func. is void */
					PRINTF_COLOR(E_FONT_RED, "Command %s exists but no function is associated to it.\r\n", command);
				}
			}
		}

		if(!cmd_match) {
			/* no matching command */
			printf("\r\nCommand \"%s\" unknown, try: help\r\n", Handle.buff);
		}

		Handle.len = 0;
		PRINT_CLI_NAME();

    }


    if(Handle.len >= MAX_LINE_LEN) {
        /* full, so restart the count */
    	PRINTF_COLOR(E_FONT_RED, "\r\nMax command length is %d.\r\n", MAX_LINE_LEN-1);
    	PRINT_CLI_NAME();
        Handle.len = 0;
    }
}


/**
  * @brief  tx handle, flushes stdout buffer
  * @param  null
  * @retval null
  */
static void cli_tx_handle(void)
{
    fflush(stdout);
}

void cli_run(void)
{
    cli_rx_handle(&cli_rx_buff);
    cli_tx_handle();
}

void greet(void){
    NL1();
    TERMINAL_BACK_DEFAULT(); /* set terminal background color: black */
    TERMINAL_DISPLAY_CLEAR();
    TERMINAL_RESET_CURSOR();
    TERMINAL_FONT_BLUE();
    printf("                             ///////////////////////////////////////////    ");NL1();
    printf("                             /////*   .////////////////////////     *///    ");NL1();
    printf("            %%%%%%         %%%%%%  ///   ////  //   //////////  //   ////   //    ");NL1();
    printf("            %%%%%%        %%%%%%   ///  //////////   ////////  ///  //////////    ");NL1();
    printf("           %%%%%%        %%%%%%%%   ((((   (((((((((   ((((((  (((((   .(((((((    ");NL1();
    printf("          %%%%%%        %%%%%%%%    (((((((    (((((((  ((((  (((((((((    ((((    ");NL1();
    printf("          %%%%%%      %%%%  %%%%    ((((((((((   ((((((  ((  ((((((((((((((  ((    ");NL1();
    printf("         %%%%%%%%    %%%%%%   %%%%%%%%  (((*((((((  .(((((((    ((((((( ((((((   ((    ");NL1();
    printf("         %%%%*%%%%%%%%%%%%           (((        (((((((((   ((((((((        ((((    ");NL1();
    printf("        %%%%   %%%%.             ###################   ##################### (((");NL1();
    printf("       %%%%%%          (((      ##################   ##################((((((( ");NL1();
    printf("       %%%%               (((( #################   ##############(((((((##    ");NL1();
    printf("      %%%%%%                   (((((((((##################((((((((((#######    ");NL1();
    printf("     %%%%%%                     ########(((((((((((((((((((################    ");NL1();
    printf("     %%%%%%                     ##%%#%%#%%#%%#%%#%%#%%#%%#%%#%%#%%#%%#%%#%%#%%#%%#%%#%%#%%#%%#%%    ");NL1();
    printf("    %%%%%%                      %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%    ");NL1();
    printf("    %%%%%%                      %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%    ");NL1();
    printf("                             %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%    ");NL1();
    printf("                             %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%    ");NL1();
    printf("µShell v0.1 - by Morgan Diepart (mdiepart@uliege.be)");NL1();
    printf("Original work from https://github.com/ShareCat/STM32CommandLine");NL1();
    printf("-------------------------------");
    NL2();
    TERMINAL_FONT_DEFAULT();
    PRINT_CLI_NAME();
    TERMINAL_SHOW_CURSOR();
}

/*************************************************************************************
 * Shell builtin functions
 ************************************************************************************/
/**
  * @brief  printf the help info.
  * @param  para addr. & length
  * @retval True means OK
  */
uint8_t cli_help(int argc, char *argv[])
{
	if(argc == 1){
	    for(size_t i = 0; i < MAX_COMMAND_NB; i++) {
	    	if(strcmp(CLI_commands[i].pCmd, "") != 0){
		    	printf("[%s]\r\n", CLI_commands[i].pCmd);
		        if (CLI_commands[i].pHelp) {
		            printf(CLI_commands[i].pHelp);NL2();
		        }
	    	}
	    }
	    return EXIT_SUCCESS;
	}else if(argc == 2){
	    for(size_t i = 0; i < MAX_COMMAND_NB; i++) {
	    	if(strcmp(CLI_commands[i].pCmd, argv[1]) == 0){
		    	printf("[%s]\r\n", CLI_commands[i].pCmd);
	    		printf(CLI_commands[i].pHelp);NL1();
	    		return EXIT_SUCCESS;
	    	}
	    }
	    printf("No help found for command %s.", argv[1]);NL1();
	    return EXIT_FAILURE;
	}else{
		printf("Command \"%s\" takes at most 1 argument.", argv[0]);NL1();
		return EXIT_FAILURE;
	}
    return EXIT_FAILURE;
}

/**
  * @brief  clear the screen
  * @param  para addr. & length
  * @retval True means OK
  */
uint8_t cli_clear(int argc, char *argv[])
{
	if(argc != 1){
		printf("command \"%s\" does not take any argument.", argv[0]);NL1();
		return EXIT_FAILURE;
	}
    TERMINAL_BACK_DEFAULT(); /* set terminal background color: black */
    TERMINAL_FONT_DEFAULT(); /* set terminal display color: green */

    /* This prints the clear screen and move cursor to top-left corner control
     * characters for VT100 terminals. This means it will not work on
     * non-VT100 compliant terminals, namely Windows' cmd.exe, but should
     * work on anything unix-y. */
    TERMINAL_RESET_CURSOR();
    TERMINAL_DISPLAY_CLEAR();

    return EXIT_SUCCESS;
}

/**
  * @brief  MCU reboot
  * @param  para addr. & length
  * @retval True means OK
  */
uint8_t cli_reboot(int argc, char *argv[])
{
	if(argc > 1){
		printf("Command \"%s\" takes no argument.", argv[0]);NL1();
		return EXIT_FAILURE;
	}

	printf("\r\n[END]: System Rebooting");
	HAL_NVIC_SystemReset();
	return EXIT_SUCCESS;
}

void cli_add_command(const char *command, const char *help, uint8_t (*exec)(int argc, char *argv[])){
	size_t i = 0;
	for(; i < MAX_COMMAND_NB; i++){
		if(strcmp(CLI_commands[i].pCmd, "") == 0){
			CLI_commands[i].pCmd = command;
			CLI_commands[i].pFun = exec;
			CLI_commands[i].pHelp = help;
			break;
		}
	}
	if(i == MAX_COMMAND_NB){
		PRINTF_COLOR(E_FONT_RED, "Cannot add command %s, max number of command reached. The maximum number of command is set to %d.\r\n", command, MAX_COMMAND_NB);
	}
}
