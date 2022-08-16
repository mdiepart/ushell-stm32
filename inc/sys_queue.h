/**
  ******************************************************************************
  * @file:      sys_queue.h
  * @author:    Cat
  * @version:   V1.0
  * @date:      2018-1-18
  * @brief:     queue
  * @attention:
  ******************************************************************************
  */


#ifndef __SYS_QUEUE_H
#define __SYS_QUEUE_H

#include <stdint.h>

#ifndef SHELL_QUEUE_LENGTH
	#define SHELL_QUEUE_LENGTH 32
#endif

typedef struct queue {
	size_t		Front;
	size_t 		Rear;
	uint8_t		PBase[SHELL_QUEUE_LENGTH];

} shell_queue_s;

uint8_t shell_queue_init(shell_queue_s *queue);
uint8_t shell_queue_full(shell_queue_s *queue);
uint8_t shell_queue_empty(shell_queue_s *queue);
uint8_t shell_queue_in(shell_queue_s *queue, uint8_t *PData);
uint8_t shell_queue_out(shell_queue_s *queue, uint8_t *PData);

#endif /* __SYS_QUEUE_H */

