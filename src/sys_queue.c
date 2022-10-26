/**
  ******************************************************************************
  * @file:      sys_queue.c
  * @author:    Cat
  * @version:   V1.0
  * @date:      2018-1-18
  * @brief:     queue
  * @attention:
  ******************************************************************************
  */

#include <string.h>
#include "main.h"
#include "../inc/sys_queue.h"
#include "stdbool.h"

/**
 * @brief  shell_queue_init inits the contents of the queue to zeros
 * @param  queue
 * @retval True
 */
uint8_t shell_queue_init(shell_queue_s *queue)
{
	queue->Front = queue->Rear = 0;

    memset(queue->PBase, 0, SHELL_QUEUE_LENGTH);

    return true;
}


/**
 * @brief  shell_queue_full checks if the queue is full
 * @param  queue
 * @retval Result of Queue Operation as bool
 */
uint8_t shell_queue_full(shell_queue_s *queue)
{
    if((((queue->Rear) + 1) % SHELL_QUEUE_LENGTH) == queue->Front) {
        return true;
    } else {
        return false;
    }
}

/**
 * @brief  shell_queue_empty checks if the queue is empty
 * @param  queue
 * @retval Result of Queue Operation as bool
 */
uint8_t shell_queue_empty(shell_queue_s *queue)
{
    if(queue->Front == queue->Rear) {
        return true;
    } else {
        return false;
    }
}


/**
 * @brief  shell_queue_in inserts a byte in the queue
 * @param  queue, PData
 * @retval Result of Queue Operation as bool
 */
uint8_t shell_queue_in(shell_queue_s *queue, uint8_t *PData)
{

    if(shell_queue_full(queue)) {
        return false;
    }

    queue->PBase[queue->Rear] = *PData;
    queue->Rear = ((queue->Rear) + 1) % SHELL_QUEUE_LENGTH;

    return true;
}


/**
 * @brief  shell_queue_out
 * @param  queue, PData
 * @retval Result of Queue Operation as bool
 */

uint8_t shell_queue_out(shell_queue_s *queue, uint8_t *PData)
{
    if(shell_queue_empty(queue)) {
        return false;
    }

    *PData = queue->PBase[queue->Front];
    queue->Front = ((queue->Front) + 1) % SHELL_QUEUE_LENGTH;

    return true;
}

