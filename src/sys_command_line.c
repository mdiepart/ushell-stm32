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
#include "../inc/sys_command_line.h"

#define RX_BUFF_TYPE        QUEUE64_S
#define HANDLE_LEN 			128

typedef struct {
    uint8_t buff[HANDLE_LEN];
    uint8_t len;
} HANDLE_TYPE_S;

typedef struct {
    const char *pCmd;
    const char *pHelp;
    uint8_t (*pFun)(void *args, uint8_t len);
} COMMAND_S;

RX_BUFF_TYPE        cli_rx_buff;
unsigned char 		cBuffer;
RX_BUFF_TYPE 		cli_rx_buff; 				/* 64 bytes FIFO, saving commands from the terminal */
UART_HandleTypeDef 	*huart_shell;
COMMAND_S			CLI_commands[MAX_COMMAND_NB];



uint8_t 	cli_help			(void *para, uint8_t len);
const char 	cli_help_help[] 		= "\r\n[help]\r\n\tshow commands\r\n";

uint8_t 	cli_clear			(void *para, uint8_t len);
const char 	cli_clear_help[] 		= "[cls]\r\n\tclear the screen\r\n";

uint8_t 	cli_reboot			(void *para, uint8_t len);
const char 	cli_reboot_help[] 		= "[reboot]\r\n\treboot MCU\r\n";

/* These functions need to be redefined over the [_weak] versions defined by GCC in
 * order to make the stdio library functional.*/
#if CLI_ENABLE
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

int _isatty(int file){
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

/*int _read(int file, char *data, int len){}*/
/*int _close(int file){}*/
/*int _lseek(int file, int ptr, int dir){}*/
/*int _fstat(int file, struct stat *st){}*/
#endif /* CLI_ENABLE */

/*
 * Command line history
 */
typedef struct {
    char cmd[HISTORY_MAX][HANDLE_LEN];
    uint8_t count;
    uint8_t latest;
    uint8_t show;
}HISTORY_S;

static HISTORY_S history;


/**
  * @brief          add a history command
  * @param  buff:   command
  * @retval         null
  */
static void cli_history_add(char* buff)
{
    uint16_t len;
    uint8_t index = history.latest;

    if (NULL == buff) return;

    len = strlen((const char *)buff);
    if (len >= HANDLE_LEN) return;  /* command too long */

    /* find the latest one */
    if (0 != index) {
        index--;
    } else {
        index = HISTORY_MAX - 1;
    }

    if (0 != memcmp(history.cmd[index], buff, len)) {
        /* if the new one is different with the latest one, the save */
        memset((void *)history.cmd[history.latest], 0x00, HANDLE_LEN);
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
  * @brief              display history command
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

    CLI_ADD_CMD("help", cli_help_help, cli_help);
    CLI_ADD_CMD("cls", cli_clear_help, cli_clear);
    CLI_ADD_CMD("reboot", cli_reboot_help, cli_reboot);

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
    printf("-------------------------------");NL3();
    TERMINAL_FONT_DEFAULT();
    PRINT_CLI_NAME();
    TERMINAL_SHOW_CURSOR();
}

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
    static HANDLE_TYPE_S Handle = {.len = 0};
    uint8_t i = Handle.len;
    uint8_t ParaLen;
    uint8_t *ParaAddr;
    uint8_t cmd_match = false;
    uint8_t exec_req = false;

    /*  ---------------------------------------
        Step1: save chars from the terminal
        ---------------------------------------
     */
    bool newChar = true;
    while(newChar) {

        if(Handle.len < HANDLE_LEN) {  /* check the buffer */
        	newChar = QUEUE_GET((*rx_buff), Handle.buff[Handle.len]);

            /* new char coming from the terminal, copy it to Handle.buff */
            if(newChar) {
                /* KEY_BACKSPACE -->get DELETE key from keyboard */
                if (Handle.buff[Handle.len] == KEY_BACKSPACE) {
                    /* buffer not empty */
                    if (Handle.len > 0) {
                        /* delete a char in terminal */
                        TERMINAL_MOVE_LEFT(1);
                        TERMINAL_CLEAR_END();
                        Handle.len--;
                    }

                } else if(Handle.buff[Handle.len] == KEY_ENTER){
                	exec_req = true;
                	Handle.len++;
                }else{
                    Handle.len++;
                }

            } else {
                /* all chars copied to Handle.buff */
                uint8_t key = 0;
                uint8_t err = 0xff;
                char *p_hist_cmd = 0;

                if (Handle.len >= 3) {
                    if (strstr((const char *)Handle.buff, KEY_UP) != NULL) {
                        key = 1;
                        TERMINAL_MOVE_LEFT(Handle.len);
                        TERMINAL_CLEAR_END();
                        err = cli_history_show(true, &p_hist_cmd);
                    } else if (strstr((const char *)Handle.buff, KEY_DOWN) != NULL) {
                        key = 2;
                        TERMINAL_MOVE_LEFT(Handle.len);
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
                            TERMINAL_MOVE_LEFT(Handle.len);
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
    if(!exec_req){
    	return;
    }else if((Handle.len == 1) && (Handle.buff[0] == KEY_ENTER)) {
        /* KEY_ENTER -->ENTER key from terminal */
    	PRINT_CLI_NAME();
        Handle.len = 0;
    } else if(Handle.len > 1) {  /* check for the length of command */
        /* a command must ending with KEY_ENTER */
        if(Handle.buff[Handle.len - 1] == KEY_ENTER) {
            Handle.buff[Handle.len - 1] = '\0';

            /* looking for a match */
            for(i = 0; i < MAX_COMMAND_NB; i++) {
                if(0 == strncmp((const char *)Handle.buff,
                                (void *)CLI_commands[i].pCmd,
                                strlen(CLI_commands[i].pCmd))) {
                    cmd_match = true;
                    ParaLen = Handle.len - strlen(CLI_commands[i].pCmd);   /* para. length */
                    ParaAddr = &Handle.buff[strlen(CLI_commands[i].pCmd)]; /* para. address */

                    if(CLI_commands[i].pFun != NULL) {
                        /* call the func. */
                    	//TERMINAL_HIDE_CURSOR();
                    	CLI_commands[i].pFun(ParaAddr, ParaLen);
                        cli_history_add((char *)Handle.buff);
                        //TERMINAL_SHOW_CURSOR();
                        break;
                    } else {
                        /* func. is void */
                        printf("\r\n-> FUNC. ERR\r\n");
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
    }


    if(Handle.len >= HANDLE_LEN) {
        /* full, so restart the count */
        Handle.len = 0;
    }
}


/**
  * @brief  tx handle
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


/*************************************************************************************
 * Shell builtin functions
 ************************************************************************************/
/**
  * @brief  printf the help info.
  * @param  para addr. & length
  * @retval True means OK
  */
uint8_t cli_help(void *para, uint8_t len)
{
    uint8_t i;

    for(i = 0; i < MAX_COMMAND_NB; i++) {
        if (CLI_commands[i].pHelp) {
            printf(CLI_commands[i].pHelp);
        }
    }

    return true;
}

/**
  * @brief  clear the screen
  * @param  para addr. & length
  * @retval True means OK
  */
uint8_t cli_clear(void *para, uint8_t len)
{
    TERMINAL_BACK_DEFAULT(); /* set terminal background color: black */
    TERMINAL_FONT_DEFAULT(); /* set terminal display color: green */

    /* This prints the clear screen and move cursor to top-left corner control
     * characters for VT100 terminals. This means it will not work on
     * non-VT100 compliant terminals, namely Windows' cmd.exe, but should
     * work on anything unix-y. */
    TERMINAL_RESET_CURSOR();
    TERMINAL_DISPLAY_CLEAR();

    return true;
}

/**
  * @brief  MCU reboot
  * @param  para addr. & length
  * @retval True means OK
  */
uint8_t cli_reboot(void *para, uint8_t len)
{
    printf("\r\n[END]: System Rebooting");
    HAL_NVIC_SystemReset();

    return true;
}

void cli_add_command(const char *command, const char *help, uint8_t (*exec)(void *args, uint8_t len)){
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
