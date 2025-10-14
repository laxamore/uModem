#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "main.h"
#include "cmsis_os.h"
#include "umodem_hal.h"
#include "umodem_buffer.h"

#define DMA_RX_BUFFER_SIZE 64

extern UART_HandleTypeDef huart2;
extern DMA_HandleTypeDef hdma_usart2_rx;

static volatile uint8_t dma_rx2_buffer[DMA_RX_BUFFER_SIZE];

static osMessageQueueId_t tx2_queue = NULL;
static osSemaphoreId_t tx2_sem = NULL;

typedef struct
{
  uint8_t *bytes;
  uint16_t len;
} tx_rx_data;

static void tx2_task(void *args)
{
  tx_rx_data data;

  while (1)
  {
    if (osMessageQueueGet(tx2_queue, &data, NULL, osWaitForever) == osOK)
    {
      if (osSemaphoreAcquire(tx2_sem, osWaitForever) != osOK)
        continue;

      huart2.pTxBuffPtr = data.bytes;
      HAL_UART_Transmit_DMA(&huart2, data.bytes, data.len);
    }

    osDelay(1);
  }
}

static volatile uint16_t start = 0;

static void process_dma_data(void)
{
  uint16_t end = DMA_RX_BUFFER_SIZE - __HAL_DMA_GET_COUNTER(&hdma_usart2_rx);

  if (end == start)
    return;

  if (end > start)
    umodem_buffer_push((const uint8_t *)&dma_rx2_buffer[start], end - start);
  else
  {
    umodem_buffer_push((const uint8_t *)&dma_rx2_buffer[start], DMA_RX_BUFFER_SIZE - start);
    umodem_buffer_push((const uint8_t *)dma_rx2_buffer, end);
  }

  start = end;
}

void umodem_hal_init(void)
{
  tx2_queue = osMessageQueueNew(16, sizeof(tx_rx_data), NULL);
  tx2_sem = osSemaphoreNew(1, 1, NULL);

  if (!tx2_sem || !tx2_queue)
    Error_Handler();

  osThreadAttr_t tx2TaskAttr = {};
  tx2TaskAttr.name = "TX2Task";
  tx2TaskAttr.priority = osPriorityNormal;
  tx2TaskAttr.stack_size = 128 * 4; // 128 words
  if (osThreadNew(tx2_task, NULL, &tx2TaskAttr) == NULL)
    Error_Handler();

  // Start DMA
  HAL_UART_Receive_DMA(&huart2, (uint8_t *)dma_rx2_buffer, DMA_RX_BUFFER_SIZE);

  // Enable IDLE interrupt
  __HAL_UART_ENABLE_IT(&huart2, UART_IT_IDLE);
}

void umodem_hal_deinit() {}

int umodem_hal_send(const uint8_t *buf, size_t len)
{
  if (len == 0)
    return len;

  tx_rx_data txrx = {0};
  txrx.bytes = calloc(len, 1);
  if (txrx.bytes == NULL)
    return -1;
  memcpy(txrx.bytes, buf, len);
  txrx.len = len;
  if (osMessageQueuePut(tx2_queue, &txrx, 0, 0) != osOK)
  {
    free(txrx.bytes);
    return -1;
  }
  return len;
}

int umodem_hal_read(uint8_t *buf, size_t len)
{
  return 0;
}

uint32_t umodem_hal_millis(void)
{
  return osKernelGetTickCount() * portTICK_PERIOD_MS;
}

void umodem_hal_delay_ms(uint32_t ms)
{
  osDelay(pdMS_TO_TICKS(ms));
}

void umodem_hal_lock(void)
{
  osSemaphoreAcquire(tx2_sem, osWaitForever);
}

void umodem_hal_unlock(void)
{
  osSemaphoreRelease(tx2_sem);
}

// --- Callbacks ---
void HAL_UART_RxHalfCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART2)
    process_dma_data();
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART2)
    process_dma_data();
}

void UART_Idle_CpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART2)
    process_dma_data();
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART2)
  {
    uint8_t *buf = (uint8_t *)huart->pTxBuffPtr;
    free(buf);
    osSemaphoreRelease(tx2_sem);
  }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART2)
  {
    // Clear error flags
    __HAL_UART_CLEAR_OREFLAG(huart);
    __HAL_UART_CLEAR_NEFLAG(huart);
    __HAL_UART_CLEAR_FEFLAG(huart);

    // Stop DMA
    HAL_UART_DMAStop(huart);

    // Restart DMA
    if (HAL_UART_Receive_DMA(&huart2, (uint8_t *)dma_rx2_buffer, DMA_RX_BUFFER_SIZE) != HAL_OK)
      Error_Handler();

    __HAL_UART_ENABLE_IT(&huart2, UART_IT_IDLE);
    start = 0;
  }
}