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
#include <errno.h>
#include <unistd.h>

#include "../shell/inc/sys_command_line.h"

static uint8_t 	cli_help(void *para, uint8_t len);
static uint8_t 	cli_clear(void *para, uint8_t len);
static uint8_t 	cli_echo(void *para, uint8_t len);
static uint8_t 	cli_reboot(void *para, uint8_t len);

unsigned char cBuffer;

typedef struct {
#define HANDLE_LEN 128

    uint8_t buff[HANDLE_LEN];
    uint8_t len;
} HANDLE_TYPE_S;



uint8_t cli_echo_flag = DISABLE; /* ECHO default: disable */

RX_BUFF_TYPE cli_rx_buff; /* 128byte FIFO, saving commands from the terminal */

UART_HandleTypeDef *huart_shell;



const char CLI_Cmd_Help[] =
    "\r\n"
    "[help]\r\n"
    " * show commands\r\n"
    "\r\n";

const char CLI_Cmd_Clear[] =
    "[cls]\r\n"
    " * clear the screen\r\n"
    "\r\n";

const char CLI_Cmd_Echo[] =
    "[echo]\r\n"
    " * echo 1: echo on\r\n"
    " * echo 0: echo off\r\n"
    "\r\n";

const char CLI_Cmd_Reboot[] =
    "[reboot]\r\n"
    " * reboot MCU\r\n"
    "\r\n";


/**
  * @brief  printf the help info.
  * @param  para addr. & length
  * @retval True means OK
  */

static uint8_t cli_help(void *para, uint8_t len)
{
    uint8_t i;

    for(i = 0; i < sizeof(CLI_Cmd) / sizeof(COMMAND_S); i++) {
        if (NULL != CLI_Cmd[i].pHelp) {
            printf(CLI_Cmd[i].pHelp);
        }
    }

    return true;
}


/**
  * @brief  clear the screen
  * @param  para addr. & length
  * @retval True means OK
  */
static uint8_t cli_clear(void *para, uint8_t len)
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
  * @brief  ECHO setting
  * @param  para addr. & length
  * @retval True means OK
  */
static uint8_t cli_echo(void *para, uint8_t len)
{
    uint8_t *pTemp;
    pTemp = (uint8_t *)para;

    if((0 < len) && (NULL != pTemp)) {
        pTemp++; /* skip a blank space*/

        if('1' == *pTemp) {
            /* ECHO on */
            cli_echo_flag = ENABLE;
            printf("echo on\r\n");
        } else if('0' == *pTemp) {
            /* ECHO off */
            cli_echo_flag = DISABLE;
            printf("echo off\r\n");
        } else {
            /* wrong para, return False */
            return false;
        }
    }

    return true;
}


/**
  * @brief  MCU reboot
  * @param  para addr. & length
  * @retval True means OK
  */
static uint8_t cli_reboot(void *para, uint8_t len)
{
    printf("\r\n[END]: System Rebooting");
    printf(".");
    HAL_Delay(200);
    printf(".");
    HAL_Delay(200);
    printf(".");
    HAL_NVIC_SystemReset();

    return true;
}


#if CLI_HISTORY

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
    //printf("history: %s \r\n", history.cmd[index]);

    return err;
}

#endif  /* CLI_HISTORY */


/**
  * @brief  command line init.
  * @param  bandrate
  * @retval null
  */
void cli_init(UART_HandleTypeDef *handle_uart)
{
	huart_shell = handle_uart;

    uint8_t i;

    memset((uint8_t *)&cli_rx_buff, 0, sizeof(RX_BUFF_TYPE));

#if CLI_HISTORY
    memset((uint8_t *)&history, 0, sizeof(history));
#endif  /* CLI_HISTORY */

    HAL_UART_MspInit(huart_shell);
    //HAL_UART_RegisterRxEventCallback(huart_shell, cli_RXEventCallback);
    HAL_UART_Receive_IT(huart_shell, &cBuffer, 1);

    /* init. every command */
    for(i = 0; i < sizeof(CLI_Cmd) / sizeof(COMMAND_S); i++) {
        /* need to init. or not */
        if(NULL != CLI_Cmd[i].pInit) {
            if(!CLI_Cmd[i].pInit()) {
                /* something wrong */
                printf("\r\n-> FUN[%d] INIT WRONG\r\n", i);
            }
        }
    }

    printf(" \r\n");
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
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef * huart){
	//HAL_GPIO_TogglePin(LD1_GPIO_Port, LD1_Pin);
	QUEUE_PUT(cli_rx_buff, cBuffer);
	HAL_UART_Receive_IT(huart, &cBuffer, 1);

}

//static void cli_RXEventCallback(UART_HandleTypeDef * huart, uint16_t Size){
//	HAL_GPIO_TogglePin(LD1_GPIO_Port, LD1_Pin);
    /*uint8_t ucData;
    if(USART_GetITStatus(DEBUG_USARTx, USART_IT_RXNE) != RESET) {
        ucData = USART_ReceiveData(DEBUG_USARTx);
        // save char
        QUEUE_IN(cli_rx_buff, ucData);
        //printf("%02x", ucTemp);
    }*/
//}

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

    /*  ---------------------------------------
        Step1: save chars from the terminal
        ---------------------------------------
     */
    while(1) {

        if(Handle.len < HANDLE_LEN) {  /* check the buffer */

            /* new char coming from the terminal, copy it to Handle.buff */
            if(QUEUE_GET((*rx_buff), Handle.buff[Handle.len])) {
                /* KEY_BACKSPACE -->get DELETE key from keyboard */
                if (KEY_BACKSPACE == Handle.buff[Handle.len]) {
                    /* buffer not empty */
                    if (0 < Handle.len) {
                        /* delete a char in terminal */
                        TERMINAL_MOVE_LEFT(1);
                        TERMINAL_CLEAR_END();
                        Handle.len -= 1;
                    }

                } else {
                    //printf("%02x ", Handle.buff[Handle.len]); /* debug */
                    Handle.len++;
                }

            } else {
                /* all chars copied to Handle.buff */
#if CLI_HISTORY
                uint8_t key = 0;
                uint8_t err = 0xff;
                char *p_hist_cmd = 0;

                if (Handle.len > 2) {
                    if (0 != strstr((const char *)Handle.buff, KEY_UP)) {
                        //printf("KEY_UP \r\n");
                        key = 1;
                        TERMINAL_MOVE_LEFT(Handle.len);
                        TERMINAL_CLEAR_END();
                        err = cli_history_show(true, &p_hist_cmd);
                    } else if (0 != strstr((const char *)Handle.buff, KEY_DOWN)) {
                        //printf("KEY_DOWN \r\n");
                        key = 2;
                        TERMINAL_MOVE_LEFT(Handle.len);
                        TERMINAL_CLEAR_END();
                        err = cli_history_show(false, &p_hist_cmd);
                    } else if (0 != strstr((const char *)Handle.buff, KEY_RIGHT)) {
                        //printf("KEY_RIGHT \r\n");
                        key = 3;
                    } else if (0 != strstr((const char *)Handle.buff, KEY_LEFT)) {
                        //printf("KEY_LEFT \r\n");
                        key = 4;
                    }

                    if (0 != key) {
                        if (false == err) {
                            memset(&Handle, 0x00, sizeof(Handle));
                            memcpy(Handle.buff, p_hist_cmd, strlen(p_hist_cmd));
                            Handle.len = strlen(p_hist_cmd);
                            Handle.buff[Handle.len] = '\0';
                            printf("%s", Handle.buff);  /* display history command */
                        } else if ((true == err) || (0 != key)) {
                            /* no history found */
                            TERMINAL_MOVE_LEFT(Handle.len);
                            TERMINAL_CLEAR_END();
                            memset(&Handle, 0x00, sizeof(Handle));
                        }
                    }
                }

                if ((0 == key) && (i < Handle.len)) {
#endif  /* CLI_HISTORY */
                    /* display char in terminal */
                    for (; i < Handle.len; i++) {
                    	HAL_UART_Transmit(huart_shell, Handle.buff+i, 1, 10);
                        //USART_SendData(DEBUG_USARTx, Handle.buff[i]);
                    }
#if CLI_HISTORY
                }
#endif  /* CLI_HISTORY */
                break;
            }

        } else {
            /* buffer full */
            break;
        }
    }

    /*  ---------------------------------------
        Step2: handle the commands
        ---------------------------------------
     */
    if((1 == Handle.len) && (KEY_ENTER == Handle.buff[Handle.len - 1])) {
        /* KEY_ENTER -->ENTER key from terminal */
        Handle.len = 0;
    } else if(1 < Handle.len) {  /* check for the length of command */
        /* a command must ending with KEY_ENTER */
        if(KEY_ENTER == Handle.buff[Handle.len - 1]) {
            Handle.buff[Handle.len - 1] = '\0';

            /* looking for a match */
            for(i = 0; i < sizeof(CLI_Cmd) / sizeof(COMMAND_S); i++) {
                if(0 == strncmp((const char *)Handle.buff,
                                (void *)CLI_Cmd[i].pCmd,
                                strlen(CLI_Cmd[i].pCmd))) {
                    cmd_match = true;
                    ParaLen = Handle.len - strlen(CLI_Cmd[i].pCmd);   /* para. length */
                    ParaAddr = &Handle.buff[strlen(CLI_Cmd[i].pCmd)]; /* para. address */

                    if(NULL != CLI_Cmd[i].pFun) {
                        /* call the func. */
                        if(CLI_Cmd[i].pFun(ParaAddr, ParaLen)) {
                            printf("\r\n-> OK\r\n");

#if CLI_HISTORY
                            cli_history_add((char *)Handle.buff);
#endif  /* CLI_HISTORY */

                            /* ECHO */
                            if(ENABLE == cli_echo_flag) {
                                Handle.buff[Handle.len] = '\0';
                                printf("[echo]: %s\r\n", (const char*)Handle.buff);
                            }
                        } else {
                            printf("\r\n-> PARA. ERR\r\n");
                            /* parameter wrong */
                            printf(CLI_Cmd[i].pHelp);
                        }
                    } else {
                        /* func. is void */
                        printf("\r\n-> FUNC. ERR\r\n");
                    }
                }
            }

            if(false == cmd_match) {
                /* no matching command */
                printf("\r\nCommand \"%s\" unknown, try: help\r\n\r\n", Handle.buff);
            }

            Handle.len = 0;

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
    /* not used for now */
}


/**
  * @brief  command line task, schedule by sys. every 50ms
  * @param  null
  * @retval null
  */
void cli_run(void)
{
    cli_rx_handle(&cli_rx_buff);
    cli_tx_handle();
}

#if CLI_PRINTF

/* These functions need to be redefined over the [_weak] versions defined by GCC in order to make the stdio library functional.*/
/*int _read(int file, char *data, int len){}*/

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


/*int _close(int file){}*/

/*int _lseek(int file, int ptr, int dir){}*/

/*int _fstat(int file, struct stat *st){}*/

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

#endif
