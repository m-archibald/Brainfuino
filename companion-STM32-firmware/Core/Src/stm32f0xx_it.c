/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32f0xx_it.c
  * @brief   Interrupt Service Routines.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32f0xx_it.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */

/* USER CODE END TD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/
extern PCD_HandleTypeDef hpcd_USB_FS;
/* USER CODE BEGIN EV */
extern uint8_t outbox[1024];
extern volatile uint32_t head;
extern volatile uint32_t tail;
extern volatile uint32_t active_mco_cfg;
extern volatile uint8_t mco_throttled;
extern volatile uint8_t active_is_tim1;
extern uint8_t cfg_uart_enabled;
extern volatile uint8_t  uart_tx_box[UART_TX_BUF_SIZE];
extern volatile uint16_t uart_tx_head;
extern volatile uint16_t uart_tx_tail;
extern uint16_t uart_pause_watermark;
extern volatile uint8_t  btn_is_pressed;
extern volatile uint32_t btn_press_tick;
extern volatile uint32_t btn_release_tick;
extern volatile uint8_t  btn_held_1s;
extern volatile uint8_t  btn_held_3s;
extern volatile uint8_t  btn_held_8s;
extern volatile uint8_t  btn_action_pending;
extern volatile uint32_t btn_action_duration;
/* USER CODE END EV */

/******************************************************************************/
/*           Cortex-M0 Processor Interruption and Exception Handlers          */
/******************************************************************************/
/**
  * @brief This function handles Non maskable interrupt.
  */
void NMI_Handler(void)
{
  /* USER CODE BEGIN NonMaskableInt_IRQn 0 */

  /* USER CODE END NonMaskableInt_IRQn 0 */
  /* USER CODE BEGIN NonMaskableInt_IRQn 1 */

  /* USER CODE END NonMaskableInt_IRQn 1 */
}

/**
  * @brief This function handles Hard fault interrupt.
  */
void HardFault_Handler(void)
{
  /* USER CODE BEGIN HardFault_IRQn 0 */

  /* USER CODE END HardFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_HardFault_IRQn 0 */
    /* USER CODE END W1_HardFault_IRQn 0 */
  }
}

/**
  * @brief This function handles System service call via SWI instruction.
  */
void SVC_Handler(void)
{
  /* USER CODE BEGIN SVC_IRQn 0 */

  /* USER CODE END SVC_IRQn 0 */
  /* USER CODE BEGIN SVC_IRQn 1 */

  /* USER CODE END SVC_IRQn 1 */
}

/**
  * @brief This function handles Pendable request for system service.
  */
void PendSV_Handler(void)
{
  /* USER CODE BEGIN PendSV_IRQn 0 */

  /* USER CODE END PendSV_IRQn 0 */
  /* USER CODE BEGIN PendSV_IRQn 1 */

  /* USER CODE END PendSV_IRQn 1 */
}

/**
  * @brief This function handles System tick timer.
  */
void SysTick_Handler(void)
{
  /* USER CODE BEGIN SysTick_IRQn 0 */

  /* USER CODE END SysTick_IRQn 0 */
  HAL_IncTick();
  /* USER CODE BEGIN SysTick_IRQn 1 */

  /* USER CODE END SysTick_IRQn 1 */
}

/******************************************************************************/
/* STM32F0xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32f0xx.s).                    */
/******************************************************************************/

/**
  * @brief This function handles EXTI line 0 and 1 interrupts.
  */
void EXTI0_1_IRQHandler(void)
{
  /* USER CODE BEGIN EXTI0_1_IRQn 0 */

  /* USER CODE END EXTI0_1_IRQn 0 */
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_1);
  /* USER CODE BEGIN EXTI0_1_IRQn 1 */

  /* USER CODE END EXTI0_1_IRQn 1 */
}

/**
  * @brief This function handles EXTI line 2 and 3 interrupts.
  */
void EXTI2_3_IRQHandler(void)
{
  /* USER CODE BEGIN EXTI2_3_IRQn 0 */
  if (EXTI->PR & EXTI_PR_PR3){
    // 1. Instantly freeze FPGA clock
    if (active_is_tim1){
      TIM1->CR1 &= ~TIM_CR1_CEN;
    } else {
      RCC->CFGR &= ~RCC_CFGR_MCO;
    }

    // 2. Settle delay (2 NOPs ensure bus propagation delay is fully satisfied)
    __NOP();
    __NOP();

    // 3. Read 8-bit output bus directly from GPIOB (PB0 - PB7)
    uint8_t data = (uint8_t)GPIOB->IDR;
    uint16_t next_tail = (tail + 1) % 1024;

    // 4. Capacity check & mirroring
    uint16_t used = (tail >= head) ? (tail - head) : (1024 - (head - tail));
    outbox[tail] = data;
    tail = next_tail;

    uint8_t throttle = (used >= (1024 - 8));

    // Mirror FPGA output byte to UART TX buffer if serial enabled
    if (cfg_uart_enabled){
      uint16_t u_next_tail = (uart_tx_tail + 1) % UART_TX_BUF_SIZE;
      uint16_t u_used = (uart_tx_tail >= uart_tx_head) ? 
                        (uart_tx_tail - uart_tx_head) : 
                        (UART_TX_BUF_SIZE - (uart_tx_head - uart_tx_tail));
      if (u_next_tail != uart_tx_head){
        uart_tx_box[uart_tx_tail] = data;
        uart_tx_tail = u_next_tail;
        USART2->CR1 |= USART_CR1_TXEIE;
      }
      if (u_used >= uart_pause_watermark){
        throttle = 1;
      }
    }

    if (!throttle){
      // Re-enable FPGA clock immediately
      if (active_is_tim1){
        TIM1->CR1 |= TIM_CR1_CEN;
      } else {
        RCC->CFGR = (RCC->CFGR & ~(RCC_CFGR_MCO | RCC_CFGR_MCOPRE)) | active_mco_cfg;
      }
    } else {
      // Outbox or UART buffer nearly full: keep FPGA clock paused until drained
      mco_throttled = 1;
    }

    // 5. Clear EXTI line 3 pending flag
    EXTI->PR = EXTI_PR_PR3;
    return;
  }
  /* USER CODE END EXTI2_3_IRQn 0 */
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_3);
  /* USER CODE BEGIN EXTI2_3_IRQn 1 */

  /* USER CODE END EXTI2_3_IRQn 1 */
}

/**
  * @brief This function handles USB global interrupt / USB wake-up interrupt through EXTI line 18.
  */
void USB_IRQHandler(void)
{
  /* USER CODE BEGIN USB_IRQn 0 */

  /* USER CODE END USB_IRQn 0 */
  HAL_PCD_IRQHandler(&hpcd_USB_FS);
  /* USER CODE BEGIN USB_IRQn 1 */

  /* USER CODE END USB_IRQn 1 */
}

/* USER CODE BEGIN 1 */

/**
  * @brief This function handles EXTI lines 4 to 15 interrupts (PC4 - BRD_RST).
  */
void EXTI4_15_IRQHandler(void)
{
  /* USER CODE BEGIN EXTI4_15_IRQn 0 */
  if (EXTI->PR & EXTI_PR_PR4){
    // 1. Clear EXTI line 4 pending flag
    EXTI->PR = EXTI_PR_PR4;

    uint32_t now = HAL_GetTick();
    uint8_t pin_state = (BRD_RST_GPIO_Port->IDR & BRD_RST_Pin) ? 1 : 0;

    if (pin_state == 0){
      // Falling edge: Button PRESSED
      // Software debounce: reject rapid contact chatter within 50ms of release
      if (!btn_is_pressed && ((now - btn_release_tick) >= 50)){
        btn_is_pressed = 1;
        btn_press_tick = now;
        btn_held_1s = 0;
        btn_held_3s = 0;
        btn_held_8s = 0;
        // Instantly halt FPGA soft-processor in hardware (< 1 µs)!
        HAL_GPIO_WritePin(BF_RST_GPIO_Port, BF_RST_Pin, GPIO_PIN_RESET);
      }
    } else {
      // Rising edge: Button RELEASED
      if (btn_is_pressed){
        uint32_t duration = now - btn_press_tick;
        if (duration >= 50){ // Software debounce: ignore contact chatter < 50ms
          btn_is_pressed = 0;
          btn_release_tick = now;
          btn_action_pending = 1;
          btn_action_duration = duration;
        }
      }
    }
    return;
  }
  /* USER CODE END EXTI4_15_IRQn 0 */
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_4);
}

/* USER CODE END 1 */
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
