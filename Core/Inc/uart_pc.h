#ifndef INC_UART_PC_H_
#define INC_UART_PC_H_

#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Work 4 — Method 2: PC truy cập SD qua UART + FATFS.
 *
 * USART1 / ST-Link VCP: 115200 8N1
 *
 * PuTTY khuyến nghị:
 *   Local echo         = Force off
 *   Local line editing = Force on
 * Đợi thấy prompt "> " rồi mới gõ lệnh tiếp.
 */

void UartPc_Init(UART_HandleTypeDef *huart);
void UartPc_Task(void *argument);
void UartPc_OnRxIrq(void);

#ifdef __cplusplus
}
#endif

#endif /* INC_UART_PC_H_ */
