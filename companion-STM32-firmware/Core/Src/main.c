/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "usbd_cdc_if.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define STRB_IDLE  		1
#define STRB_PULSE 		0
#define T 				10
#define STATE_RUN 		0
#define STATE_PROGRAM 	1
#define OUTBOX_CAPACITY 1024

#define PROG_BUF_SIZE           4096
#define STAGING_BUF_SIZE        1024
#define FLASH_CAPACITY          262144
#define PROG_IDLE_TIMEOUT_MS    100
#define STREAM_REPORT_INTERVAL  1024

// Configurable features (ready for upcoming interactive terminal menu)
#define CFG_AUTO_PROG_RUN_MODE   1   // 1 = Auto-enter program mode if code paste detected in RUN mode
#define CFG_AUTO_PROG_MIN_BYTES  16  // Minimum packet size to trigger upload (safe against 3-5 byte ANSI escape keys)
#define CFG_AUTO_RESET_AFTER_PGM 1   // 1 = Automatically reset and launch program after write finishes
#define CFG_APPEND_ENDLESS_LOOP  1   // 1 = Automatically append '[-]+[]' infinite loop at EOF to halt PC cleanly

// Run Mode speed hotkey options
#define SPEED_HOTKEY_NONE        0   // Disabled
#define SPEED_HOTKEY_PGUP_PGDN   1   // Page Up / Page Down (\x1b[5~ / \x1b[6~)
#define SPEED_HOTKEY_UP_DOWN     2   // Up Arrow / Down Arrow (\x1b[A / \x1b[B)
#define SPEED_HOTKEY_PLUS_MINUS  3   // '+' / '-'

#define CFG_SPEED_HOTKEY_MODE    SPEED_HOTKEY_PGUP_PGDN

#define DFU_MAGIC_ADDR          (*((volatile uint32_t *)0x20003FF0))
#define DFU_MAGIC_VALUE         0xDEADBEEF
#define STM32F072_ROM_BASE      0x1FFFC800

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

//get a bit from a variable
#define GETBIT(var, bit)    (((var) >> (bit)) & 1)

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc;

/* USER CODE BEGIN PV */

extern USBD_HandleTypeDef hUsbDeviceFS;
uint8_t out_pending;
uint8_t code_dump;
uint8_t state;
uint8_t freq;
uint8_t serial_input;
uint8_t board_incoming;
uint8_t adc_input;
uint8_t ADC_ON;
uint32_t code_size;
uint8_t outbox[OUTBOX_CAPACITY];
uint32_t head;
uint32_t leaving;
uint32_t tail;
uint32_t tail_temp;

volatile uint8_t dfu_requested;

// Button debounce and hold tracking
uint8_t btn_last_state;
uint32_t btn_press_tick;
uint8_t btn_held_3s;
uint8_t btn_held_10s;

// Dedicated program mode buffers and state
uint8_t prog_buffer[PROG_BUF_SIZE];
volatile uint8_t stream_staging[STAGING_BUF_SIZE];
volatile uint16_t staging_head;
volatile uint16_t staging_tail;
volatile uint32_t prog_rx_count;
volatile uint32_t prog_total_received;
volatile uint32_t prog_flash_addr;
volatile uint32_t prog_last_rx_tick;
volatile uint8_t prog_active;
volatile uint8_t prog_is_streaming;
uint32_t prog_next_report_addr;
uint8_t stream_init_done;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC_Init(void);
/* USER CODE BEGIN PFP */

void initROMNormal(void);
uint8_t readBFOutput(void);
uint8_t wait(uint32_t);
void writeBFInput(uint8_t);
uint8_t readROM(uint32_t);
uint8_t writeROM(uint32_t, uint8_t);
uint8_t eraseROM(void);
void initROMWrite(void);

void CDC_Print(const char *str);
void CDC_Printf(const char *format, ...);
uint8_t TxBusy(void);
void initROMProgramming(void);
void setAddrBusFast(uint32_t addr);
void setDataBusFast(uint8_t data);
uint8_t readDataBusFast(void);
void setDataBusModeInput(void);
void setDataBusModeOutput(void);
uint8_t LLReadROMFast(uint32_t addr);
void LLWriteROMFast(uint32_t addr, uint8_t data);
uint8_t ToggleBitWaitFast(uint32_t addr);
uint8_t writeROMFast(uint32_t addr, uint8_t data);
uint8_t eraseROMFast(void);
uint8_t flashBufferToROM(const uint8_t *code, uint32_t len, const char *title, uint8_t append_loop, uint8_t auto_launch);
void flashDefaultLogoProgram(void);
void Execute_DFU_Jump(void);
const char *get_freq_name(uint8_t f);
void speed_step_up(void);
void speed_step_down(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin){

  if(GPIO_Pin == GPIO_PIN_3){  //OutStrobe
	  //outbox[tail] = readBFOutput();
	  //tail = (tail==(OUTBOX_CAPACITY-1))?0:(tail + 1);
	  outbox[tail] = (uint8_t)GPIOB->IDR;
	  tail = (tail+1) % OUTBOX_CAPACITY;
  }
  else
	  if (GPIO_Pin == GPIO_PIN_1){  //InStrobe
		  HAL_GPIO_WritePin(BF_INCMG_GPIO_Port, BF_INCMG_Pin, GPIO_PIN_RESET);
		  serial_input = 0;
  }
}

void set_freq(uint8_t f){
	// ICS VERSION
	//return;
	switch(f){
		case '1': // 500kHz
			  HAL_RCC_MCOConfig(RCC_MCO, RCC_MCO1SOURCE_HSI, RCC_MCODIV_16);
			break;
		case '2': // 3MHz
			HAL_RCC_MCOConfig(RCC_MCO, RCC_MCO1SOURCE_HSI48, RCC_MCODIV_16);
			break;
		case '3': // 6MHz
		    HAL_RCC_MCOConfig(RCC_MCO, RCC_MCO1SOURCE_HSI48, RCC_MCODIV_8);
			break;
		case '4': // 8MHz
			  HAL_RCC_MCOConfig(RCC_MCO, RCC_MCO1SOURCE_HSI, RCC_MCODIV_1);
			break;
		case '5': // 12MHz
			HAL_RCC_MCOConfig(RCC_MCO, RCC_MCO1SOURCE_HSI48, RCC_MCODIV_4);
			break;
		case '6': // 24MHz
			HAL_RCC_MCOConfig(RCC_MCO, RCC_MCO1SOURCE_HSI48, RCC_MCODIV_2);
			break;
		case '7': // 48MHz
			HAL_RCC_MCOConfig(RCC_MCO, RCC_MCO1SOURCE_HSI48, RCC_MCODIV_1);
			break;
	}

}

const char *get_freq_name(uint8_t f){
	switch(f){
		case '1': return "500 kHz";
		case '2': return "3 MHz";
		case '3': return "6 MHz";
		case '4': return "8 MHz";
		case '5': return "12 MHz";
		case '6': return "24 MHz";
		case '7': return "48 MHz";
		default:  return "Unknown";
	}
}

void speed_step_up(void){
	if (freq < '7'){
		freq++;
		set_freq(freq);
		// Transient overlay: save cursor, print dimmed clock status, restore cursor so BF code overwrites it naturally
		CDC_Printf("\x1b[s\x1b[2m[Clock: %s]\x1b[0m\x1b[u", get_freq_name(freq));
		HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
		for (volatile int i = 0; i < 60000; i++);
		HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
	}
}

void speed_step_down(void){
	if (freq > '1'){
		freq--;
		set_freq(freq);
		// Transient overlay: save cursor, print dimmed clock status, restore cursor so BF code overwrites it naturally
		CDC_Printf("\x1b[s\x1b[2m[Clock: %s]\x1b[0m\x1b[u", get_freq_name(freq));
		HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
		for (volatile int i = 0; i < 60000; i++);
		HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
	}
}

void CDC_Print(const char *str){
	uint32_t len = strlen(str);
	uint32_t offset = 0;
	while (offset < len){
		uint16_t chunk = (len - offset > 64) ? 64 : (uint16_t)(len - offset);
		uint32_t timeout = 200000;
		while (TxBusy() && --timeout);
		CDC_Transmit_FS((uint8_t *)(str + offset), chunk);
		timeout = 200000;
		while (TxBusy() && --timeout);
		offset += chunk;
	}
}

void CDC_Printf(const char *format, ...){
	char msg[128];
	va_list args;
	va_start(args, format);
	vsnprintf(msg, sizeof(msg), format, args);
	va_end(args);
	CDC_Print(msg);
}

uint8_t CDC_Receive_Callback(uint8_t *buff, uint32_t len){
	// Automated USB DFU trigger command (from build script or terminal)
	if (len >= 5 && strncmp((char *)buff, "!DFU!", 5) == 0){
		dfu_requested = 1;
		return 1;
	}

	if (state == STATE_RUN){
		if (CFG_AUTO_PROG_RUN_MODE && (len >= CFG_AUTO_PROG_MIN_BYTES)){
			// Auto-detected code paste in Run Mode! Transition to Program Mode
			state = STATE_PROGRAM;
			HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
			HAL_GPIO_WritePin(BF_RST_GPIO_Port, BF_RST_Pin, GPIO_PIN_RESET); // Hold soft-processor in reset

			prog_rx_count = 0;
			prog_total_received = 0;
			prog_flash_addr = 0;
			prog_active = 1;
			prog_is_streaming = 0;
			staging_head = 0;
			staging_tail = 0;
			stream_init_done = 0;
			prog_last_rx_tick = HAL_GetTick();

			memcpy(prog_buffer, buff, len);
			prog_rx_count = len;
			prog_total_received = len;

			CDC_Print("\r\n=== AUTO-PROGRAM MODE ===\r\nUploading Brainfuck code...\r\n");
			return 1;
		}

		// Run Mode dynamic speed switching hotkeys
		if (CFG_SPEED_HOTKEY_MODE == SPEED_HOTKEY_PGUP_PGDN){
			if (len == 4 && buff[0] == 0x1B && buff[1] == '[' && buff[2] == '5' && buff[3] == '~'){
				speed_step_up();
				return 1;
			}
			if (len == 4 && buff[0] == 0x1B && buff[1] == '[' && buff[2] == '6' && buff[3] == '~'){
				speed_step_down();
				return 1;
			}
		}
		else if (CFG_SPEED_HOTKEY_MODE == SPEED_HOTKEY_UP_DOWN){
			if (len == 3 && buff[0] == 0x1B && (buff[1] == '[' || buff[1] == 'O') && buff[2] == 'A'){
				speed_step_up();
				return 1;
			}
			if (len == 3 && buff[0] == 0x1B && (buff[1] == '[' || buff[1] == 'O') && buff[2] == 'B'){
				speed_step_down();
				return 1;
			}
		}
		else if (CFG_SPEED_HOTKEY_MODE == SPEED_HOTKEY_PLUS_MINUS){
			if (len == 1 && buff[0] == '+'){
				speed_step_up();
				return 1;
			}
			if (len == 1 && buff[0] == '-'){
				speed_step_down();
				return 1;
			}
		}

		// Pass characters cleanly to the running FPGA soft-processor without interception
		if (len >= 1){
			writeBFInput(*buff);
			HAL_GPIO_WritePin(BF_INCMG_GPIO_Port, BF_INCMG_Pin, GPIO_PIN_SET);
			serial_input = 1;
		}
		return 1;
	}

	if (state == STATE_PROGRAM){
		prog_last_rx_tick = HAL_GetTick();
		prog_active = 1;

		if (!prog_is_streaming){
			if (prog_rx_count + len <= PROG_BUF_SIZE){
				memcpy(prog_buffer + prog_rx_count, buff, len);
				prog_rx_count += len;
				prog_total_received += len;
				return 1; // Buffer has room: re-arm USB endpoint immediately
			}
			else {
				// Buffer has filled 4 kB: fill remainder of prog_buffer and transition to streaming
				uint32_t space = PROG_BUF_SIZE - prog_rx_count;
				if (space > 0){
					memcpy(prog_buffer + prog_rx_count, buff, space);
					prog_rx_count += space;
					prog_total_received += space;
				}
				prog_is_streaming = 1;
				uint32_t rem = len - space;
				for (uint32_t i = 0; i < rem; i++){
					uint16_t next = (staging_head + 1) % STAGING_BUF_SIZE;
					if (next != staging_tail){
						stream_staging[staging_head] = buff[space + i];
						staging_head = next;
						prog_total_received++;
					}
				}

				// Pause USB reception immediately: host pauses while main() erases ROM and writes first 4 kB!
				cdc_rx_paused = 1;
				return 0; // Hardware NAK backpressure!
			}
		}
		else {
			// In streaming mode: queue incoming packets into staging FIFO
			for (uint32_t i = 0; i < len; i++){
				if (prog_total_received < FLASH_CAPACITY){
					uint16_t next = (staging_head + 1) % STAGING_BUF_SIZE;
					if (next != staging_tail){
						stream_staging[staging_head] = buff[i];
						staging_head = next;
						prog_total_received++;
					}
				}
			}

			// Throttle endpoint if staging FIFO is nearing capacity
			uint16_t staging_used = (staging_head >= staging_tail) ? (staging_head - staging_tail) : (STAGING_BUF_SIZE - (staging_tail - staging_head));
			uint16_t staging_free = (STAGING_BUF_SIZE - 1) - staging_used;
			if (staging_free >= 128){
				return 1; // Room for next packet
			} else {
				cdc_rx_paused = 1;
				return 0; // Throttle USB endpoint (return NAK to host)
			}
		}
	}

	return 1;
}

uint8_t TxBusy(){
  USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef*)hUsbDeviceFS.pClassData;
  if (hcdc->TxState != 0)
	  return 1;
  return 0;
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USB_DEVICE_Init();
  MX_ADC_Init();
  /* USER CODE BEGIN 2 */

  // Bootup alive blip on Red LED
  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
  HAL_Delay(80);
  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);

  serial_input = 0;
  head = 0;
  leaving = 0;
  tail = 0;

  ADC_ON = 0;

  code_size = 0;
  initROMNormal();
  wait(1000);
  HAL_GPIO_WritePin(OE_GPIO_Port, OE_Pin, GPIO_PIN_RESET);
  wait(1000);
  HAL_GPIO_WritePin(BF_RST_GPIO_Port, BF_RST_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
  state = STATE_RUN;

  // Default clock frequency: 500 kHz
  freq = '1';
  set_freq(freq);

  btn_last_state = 1;
  btn_press_tick = 0;
  btn_held_3s = 0;
  btn_held_10s = 0;

  prog_rx_count = 0;
  prog_total_received = 0;
  prog_flash_addr = 0;
  prog_last_rx_tick = 0;
  prog_active = 0;
  prog_is_streaming = 0;
  staging_head = 0;
  staging_tail = 0;
  stream_init_done = 0;

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  while (1){
	  // Check for DFU jump request (executed in Thread Mode outside interrupt)
	  if (dfu_requested){
		  dfu_requested = 0;
		  Execute_DFU_Jump();
	  }

	  // 1. Debounced button state machine
	  uint8_t btn_curr = HAL_GPIO_ReadPin(BRD_RST_GPIO_Port, BRD_RST_Pin);

	  if (btn_curr == GPIO_PIN_RESET){
		  // Button is pressed (active LOW)
		  if (btn_last_state == GPIO_PIN_SET){
			  // Button just pressed down
			  btn_press_tick = HAL_GetTick();
			  btn_held_3s = 0;
			  btn_held_10s = 0;
			  // Hold FPGA in reset while physical button is held down
			  HAL_GPIO_WritePin(BF_RST_GPIO_Port, BF_RST_Pin, GPIO_PIN_RESET);
		  }
		  else {
			  // Button is being held down
			  uint32_t hold_time = HAL_GetTick() - btn_press_tick;
			  if (hold_time >= 10000){
				  btn_held_10s = 1;
				  // Visual indication: rapid Red LED strobe (toggle every 100ms) at 10s mark
				  if ((hold_time / 100) % 2){
					  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
				  } else {
					  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
				  }
			  }
			  else if (hold_time >= 3000){
				  btn_held_3s = 1;
				  // Visual indication: Red LED turns ON solid at 3.0s!
				  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
			  }
		  }
	  }
	  else {
		  // Button is unpressed (HIGH)
		  if (btn_last_state == GPIO_PIN_RESET){
			  // Button just released!
			  uint32_t press_duration = HAL_GetTick() - btn_press_tick;

			  if (btn_held_10s || (press_duration >= 10000)){
				  // Held >= 10s: Restore Default Brainfuino ASCII Logo Demo!
				  flashDefaultLogoProgram();
			  }
			  else if (btn_held_3s || (press_duration >= 3000)){
				  // Held >= 3s: Enter Program Mode
				  state = STATE_PROGRAM;
				  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
				  HAL_GPIO_WritePin(BF_RST_GPIO_Port, BF_RST_Pin, GPIO_PIN_RESET); // Hold soft-processor in reset

				  prog_rx_count = 0;
				  prog_total_received = 0;
				  prog_flash_addr = 0;
				  prog_active = 0;
				  prog_is_streaming = 0;
				  staging_head = 0;
				  staging_tail = 0;
				  stream_init_done = 0;

				  CDC_Print("\r\n\r\n=== BRAINFUINO PROGRAM MODE ===\r\nPaste Brainfuck code now (up to 256 kB)...\r\n");
				  CDC_Resume_Rx();
			  }
			  else if (press_duration > 20){
				  // Short press (< 3s)
				  if (state == STATE_PROGRAM){
					  // In Program Mode: short press exits Program Mode and runs the program
					  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
					  initROMNormal();
					  wait(1000);
					  HAL_GPIO_WritePin(OE_GPIO_Port, OE_Pin, GPIO_PIN_RESET);
					  wait(1000);
					  // Pulse FPGA reset
					  HAL_GPIO_WritePin(BF_RST_GPIO_Port, BF_RST_Pin, GPIO_PIN_RESET);
					  HAL_Delay(10);
					  HAL_GPIO_WritePin(BF_RST_GPIO_Port, BF_RST_Pin, GPIO_PIN_SET);
					  state = STATE_RUN;
					  CDC_Print("\r\n[Running program]\r\n");
				  }
				  else {
					  // In Run Mode: short press resets the running soft-processor
					  HAL_GPIO_WritePin(BF_RST_GPIO_Port, BF_RST_Pin, GPIO_PIN_RESET);
					  HAL_Delay(10);
					  HAL_GPIO_WritePin(BF_RST_GPIO_Port, BF_RST_Pin, GPIO_PIN_SET);
					  CDC_Print("\r\n[Reset]\r\n");
				  }
			  }
			  btn_held_3s = 0;
			  btn_held_10s = 0;
		  }
	  }
	  btn_last_state = btn_curr;

	  // 2. Program Mode flashing handling
	  if (state == STATE_PROGRAM){
		  if (prog_is_streaming){
			  if (!stream_init_done){
				  CDC_Print("\r\nProgram larger than RAM buffer. Streaming directly to Flash...\r\nErasing ROM... ");
				  initROMProgramming();
				  eraseROMFast();
				  CDC_Print("Done.\r\n");

				  // Flush initial bytes from prog_buffer to Flash
				  for (uint32_t i = 0; i < PROG_BUF_SIZE; i++){
					  writeROMFast(i, prog_buffer[i]);
				  }
				  prog_flash_addr = PROG_BUF_SIZE;
				  CDC_Printf("Wrote %lu bytes...\r\n", prog_flash_addr);
				  prog_next_report_addr = PROG_BUF_SIZE + STREAM_REPORT_INTERVAL;
				  stream_init_done = 1;

				  // Initial stall complete: Resume USB reception to stream the remainder!
				  CDC_Resume_Rx();
			  }

			  // Drain staging FIFO to Flash
			  while (staging_head != staging_tail){
				  uint8_t b = stream_staging[staging_tail];
				  staging_tail = (staging_tail + 1) % STAGING_BUF_SIZE;

				  if (prog_flash_addr < FLASH_CAPACITY){
					  writeROMFast(prog_flash_addr++, b);
					  if (prog_flash_addr >= prog_next_report_addr){
						  CDC_Printf("Wrote %lu bytes...\r\n", prog_flash_addr);
						  prog_next_report_addr += STREAM_REPORT_INTERVAL;
					  }
				  }
				  else if (prog_flash_addr == FLASH_CAPACITY){
					  CDC_Print("\r\nWarning: Flash capacity (256 kB) reached! Truncating.\r\n");
					  prog_flash_addr++;
				  }

				  // Resume USB RX if it was paused and staging now has plenty of room
				  if (cdc_rx_paused){
					  uint16_t staging_used = (staging_head >= staging_tail) ? (staging_head - staging_tail) : (STAGING_BUF_SIZE - (staging_tail - staging_head));
					  uint16_t staging_free = (STAGING_BUF_SIZE - 1) - staging_used;
					  if (staging_free >= 256){
						  CDC_Resume_Rx();
					  }
				  }
			  }

			  // Always ensure USB RX resumes if staging is completely drained
			  if (cdc_rx_paused){
				  CDC_Resume_Rx();
			  }

			  // Idle timeout detection in streaming mode
			  if (stream_init_done && (staging_head == staging_tail) && ((HAL_GetTick() - prog_last_rx_tick) >= PROG_IDLE_TIMEOUT_MS)){
				  if (CFG_APPEND_ENDLESS_LOOP && (prog_flash_addr + 6 <= FLASH_CAPACITY)){
					  writeROMFast(prog_flash_addr++, '[');
					  writeROMFast(prog_flash_addr++, '-');
					  writeROMFast(prog_flash_addr++, ']');
					  writeROMFast(prog_flash_addr++, '+');
					  writeROMFast(prog_flash_addr++, '[');
					  writeROMFast(prog_flash_addr++, ']');
				  }

				  uint32_t written = (prog_flash_addr > FLASH_CAPACITY) ? FLASH_CAPACITY : prog_flash_addr;
				  CDC_Printf("\r\nFinished! Wrote %lu bytes total.\r\n", written);

				  if (CFG_AUTO_RESET_AFTER_PGM){
					  CDC_Print("Auto-launching program...\r\n");
					  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
					  initROMNormal();
					  wait(1000);
					  HAL_GPIO_WritePin(OE_GPIO_Port, OE_Pin, GPIO_PIN_RESET);
					  wait(1000);
					  HAL_GPIO_WritePin(BF_RST_GPIO_Port, BF_RST_Pin, GPIO_PIN_RESET);
					  HAL_Delay(10);
					  HAL_GPIO_WritePin(BF_RST_GPIO_Port, BF_RST_Pin, GPIO_PIN_SET);
					  state = STATE_RUN;
				  }
				  else {
					  CDC_Print("Press the Reset button to run the program.\r\n");
				  }

				  prog_active = 0;
				  prog_is_streaming = 0;
				  stream_init_done = 0;
				  prog_rx_count = 0;
				  CDC_Resume_Rx();
			  }
		  }
		  else if (prog_active && ((HAL_GetTick() - prog_last_rx_tick) >= PROG_IDLE_TIMEOUT_MS)){
			  // Program <= 4 kB: paste complete! Flash via unified engine
			  flashBufferToROM(prog_buffer, prog_rx_count, NULL, CFG_APPEND_ENDLESS_LOOP, CFG_AUTO_RESET_AFTER_PGM);

			  prog_active = 0;
			  prog_rx_count = 0;
			  prog_total_received = 0;
			  prog_flash_addr = 0;
			  prog_is_streaming = 0;
			  staging_head = 0;
			  staging_tail = 0;
			  stream_init_done = 0;
			  CDC_Resume_Rx();
		  }
	  }

	  // Stop BF clock when BF output buffer is full
	  if( ((tail < leaving) && ((leaving-tail) < 3)) ||
		((leaving < 4) && (tail > OUTBOX_CAPACITY-4))  ){
		  //pause_clock()vvvvvvvvvvv
		  HAL_RCC_MCOConfig(RCC_MCO, RCC_MCO1SOURCE_NOCLOCK, RCC_MCODIV_1);
		  // ICS VERSION
	  	  //HAL_GPIO_WritePin(BF_CLK_INH_GPIO_Port,BF_CLK_INH_Pin,GPIO_PIN_SET);
	  }

	  // Send data out to host computer
	  if((head != tail) && !TxBusy()){
		  HAL_NVIC_DisableIRQ(EXTI2_3_IRQn);
		  tail_temp = tail;                  // Critical Section
	  	  HAL_NVIC_EnableIRQ(EXTI2_3_IRQn);
		  leaving = head;
	  	  if(head < tail_temp){
	  		  if( (tail_temp-head) < 60 ){ // easy case
				  CDC_Transmit_FS(outbox+head, tail_temp-head);
				  head = tail_temp;
	  		  }
	  		  else{ 				// chunk it
				  CDC_Transmit_FS(outbox+head, 60);
				  head += 60;
	  		  }
	  	  }
	  	  else{
	  		  if(head >= (OUTBOX_CAPACITY-60)){ // easy case
				  CDC_Transmit_FS(outbox+head, OUTBOX_CAPACITY-head);
				  head = 0;
	  		  }
	  		  else{ 				// chunk it
				  CDC_Transmit_FS(outbox+head, 60);
				  head += 60;
	  		  }
	  	  }
		  //resume_clock()vvvvvvvv
	  	  set_freq(freq);
	  	  //ICS VERSION
	  	  //HAL_GPIO_WritePin(BF_CLK_INH_GPIO_Port,BF_CLK_INH_Pin,GPIO_PIN_RESET);
	  }

	  // update hardware input (digital or analog)
	  __disable_irq();
	  if(!serial_input){
		  __enable_irq();
		  board_incoming = HAL_GPIO_ReadPin(BRD_INCMG_GPIO_Port, BRD_INCMG_Pin);
		  if(board_incoming){
			  __disable_irq();
			  writeBFInput((uint8_t)GPIOD->IDR);
			  HAL_GPIO_WritePin(BF_INCMG_GPIO_Port, BF_INCMG_Pin, GPIO_PIN_SET);
			  __enable_irq();
		  }
		  else{
			  adc_input = HAL_GPIO_ReadPin(AIEN_GPIO_Port, AIEN_Pin);
			  if(adc_input){
				  if(!ADC_ON){
					  HAL_ADC_Start(&hadc);
					  HAL_ADC_PollForConversion(&hadc, 1);
					  ADC_ON = 1;
				  }
				  __disable_irq();
				  writeBFInput((uint8_t)HAL_ADC_GetValue(&hadc));
				  HAL_GPIO_WritePin(BF_INCMG_GPIO_Port, BF_INCMG_Pin, GPIO_PIN_SET);
				  __enable_irq();
			  }
			  else{
				  HAL_GPIO_WritePin(BF_INCMG_GPIO_Port, BF_INCMG_Pin, GPIO_PIN_RESET);
			  }
		  }
	  }
	  __enable_irq();

	  // dump the code
	  if(code_dump){
		  uint32_t i,j;
		  const uint32_t N = 5000;
		  uint8_t code_str[N];

		  HAL_GPIO_WritePin(BF_RST_GPIO_Port, BF_RST_Pin, GPIO_PIN_RESET);
		  HAL_Delay(10);

		  for(i = 0; i < N; i++){
			  code_str[i] = readROM(i);
			  if (code_str[i] == 0xFF) break;
		  }

		  for(j = 0; (i-j) >= 60; j+=60){
			  while(TxBusy()){};
			  CDC_Transmit_FS(code_str+j, 60);
		  }
		  if(j<i){
			  while(TxBusy()){};
			  CDC_Transmit_FS(code_str+j, i-j);
		  }

		  if(state==STATE_RUN){
			  initROMNormal();
			  HAL_GPIO_WritePin(OE_GPIO_Port, OE_Pin, GPIO_PIN_RESET);
			  HAL_Delay(10);
			  if(HAL_GPIO_ReadPin(BRD_RST_GPIO_Port, BRD_RST_Pin))  // if RST button pressed, don't un-reset
				  HAL_GPIO_WritePin(BF_RST_GPIO_Port, BF_RST_Pin, GPIO_PIN_SET);
			  HAL_Delay(1);
		  }

		  code_dump = 0;
	  }

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
  RCC_CRSInitTypeDef RCC_CRSInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_HSI14
                              |RCC_OSCILLATORTYPE_HSI48;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
  RCC_OscInitStruct.HSI14State = RCC_HSI14_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.HSI14CalibrationValue = 16;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI48;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USB;
  PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_HSI48;

  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
  HAL_RCC_MCOConfig(RCC_MCO, RCC_MCO1SOURCE_HSI, RCC_MCODIV_16);
  /** Enable the SYSCFG APB clock
  */
  __HAL_RCC_CRS_CLK_ENABLE();
  /** Configures CRS
  */
  RCC_CRSInitStruct.Prescaler = RCC_CRS_SYNC_DIV1;
  RCC_CRSInitStruct.Source = RCC_CRS_SYNC_SOURCE_USB;
  RCC_CRSInitStruct.Polarity = RCC_CRS_SYNC_POLARITY_RISING;
  RCC_CRSInitStruct.ReloadValue = __HAL_RCC_CRS_RELOADVALUE_CALCULATE(48000000,1000);
  RCC_CRSInitStruct.ErrorLimitValue = 34;
  RCC_CRSInitStruct.HSI48CalibrationValue = 32;

  HAL_RCCEx_CRSConfig(&RCC_CRSInitStruct);
}

/**
  * @brief ADC Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC_Init(void)
{

  /* USER CODE BEGIN ADC_Init 0 */

  /* USER CODE END ADC_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC_Init 1 */

  /* USER CODE END ADC_Init 1 */
  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc.Instance = ADC1;
  hadc.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV1;
  hadc.Init.Resolution = ADC_RESOLUTION_8B;
  hadc.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc.Init.ScanConvMode = ADC_SCAN_DIRECTION_FORWARD;
  hadc.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc.Init.LowPowerAutoWait = DISABLE;
  hadc.Init.LowPowerAutoPowerOff = DISABLE;
  hadc.Init.ContinuousConvMode = ENABLE;
  hadc.Init.DiscontinuousConvMode = DISABLE;
  hadc.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc.Init.DMAContinuousRequests = DISABLE;
  hadc.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
  if (HAL_ADC_Init(&hadc) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure for the selected ADC regular channel to be converted.
  */
  sConfig.Channel = ADC_CHANNEL_10;
  sConfig.Rank = ADC_RANK_CHANNEL_NUMBER;
  sConfig.SamplingTime = ADC_SAMPLETIME_28CYCLES_5;
  if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC_Init 2 */

  /* USER CODE END ADC_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, BRD_INSTRB_Pin|OE_Pin|WE_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, BF_IN0_Pin|BF_IN1_Pin|BF_IN2_Pin|BF_IN3_Pin
                          |BF_IN4_Pin|BF_IN5_Pin|BF_IN6_Pin|BF_IN7_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, BF_INCMG_Pin|BF_RST_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : A2_Pin A3_Pin A4_Pin A5_Pin
                           A6_Pin A7_Pin A8_Pin A9_Pin
                           A10_Pin A11_Pin A12_Pin A13_Pin
                           A14_Pin A15_Pin A0_Pin A1_Pin */
  GPIO_InitStruct.Pin = A2_Pin|A3_Pin|A4_Pin|A5_Pin
                          |A6_Pin|A7_Pin|A8_Pin|A9_Pin
                          |A10_Pin|A11_Pin|A12_Pin|A13_Pin
                          |A14_Pin|A15_Pin|A0_Pin|A1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pin : LED_Pin */
  GPIO_InitStruct.Pin = LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : BF_INSTRB_Pin */
  GPIO_InitStruct.Pin = BF_INSTRB_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(BF_INSTRB_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : BRD_INSTRB_Pin */
  GPIO_InitStruct.Pin = BRD_INSTRB_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(BRD_INSTRB_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : BF_OUTSTRB_Pin */
  GPIO_InitStruct.Pin = BF_OUTSTRB_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(BF_OUTSTRB_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : BF_IN0_Pin BF_IN1_Pin BF_IN2_Pin BF_IN3_Pin
                           BF_IN4_Pin BF_IN5_Pin BF_IN6_Pin BF_IN7_Pin */
  GPIO_InitStruct.Pin = BF_IN0_Pin|BF_IN1_Pin|BF_IN2_Pin|BF_IN3_Pin
                          |BF_IN4_Pin|BF_IN5_Pin|BF_IN6_Pin|BF_IN7_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : BRD_RST_Pin */
  GPIO_InitStruct.Pin = BRD_RST_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(BRD_RST_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : AIEN_Pin BRD_INCMG_Pin */
  GPIO_InitStruct.Pin = AIEN_Pin|BRD_INCMG_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : BF_OUT0_Pin BF_OUT1_Pin BF_OUT2_Pin D2_Pin
                           D3_Pin D4_Pin D5_Pin D6_Pin
                           D7_Pin BF_OUT3_Pin BF_OUT4_Pin BF_OUT5_Pin
                           BF_OUT6_Pin BF_OUT7_Pin D0_Pin D1_Pin */
  GPIO_InitStruct.Pin = BF_OUT0_Pin|BF_OUT1_Pin|BF_OUT2_Pin|D2_Pin
                          |D3_Pin|D4_Pin|D5_Pin|D6_Pin
                          |D7_Pin|BF_OUT3_Pin|BF_OUT4_Pin|BF_OUT5_Pin
                          |BF_OUT6_Pin|BF_OUT7_Pin|D0_Pin|D1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : BF_INCMG_Pin BF_RST_Pin */
  GPIO_InitStruct.Pin = BF_INCMG_Pin|BF_RST_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : A16_Pin A17_Pin */
  GPIO_InitStruct.Pin = A16_Pin|A17_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : BF_CLK_Pin */
  GPIO_InitStruct.Pin = BF_CLK_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF0_MCO;
  HAL_GPIO_Init(BF_CLK_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : OE_Pin WE_Pin */
  GPIO_InitStruct.Pin = OE_Pin|WE_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : IN0_Pin IN1_Pin IN2_Pin IN3_Pin
                           IN4_Pin IN5_Pin IN6_Pin IN7_Pin */
  GPIO_InitStruct.Pin = IN0_Pin|IN1_Pin|IN2_Pin|IN3_Pin
                          |IN4_Pin|IN5_Pin|IN6_Pin|IN7_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI0_1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI0_1_IRQn);

  HAL_NVIC_SetPriority(EXTI2_3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI2_3_IRQn);

}

/* USER CODE BEGIN 4 */

void initROMNormal(){
	  GPIO_InitTypeDef GPIO_InitStruct = {0};

	  HAL_GPIO_WritePin(OE_GPIO_Port, OE_Pin, GPIO_PIN_SET);
	  HAL_GPIO_WritePin(WE_GPIO_Port, WE_Pin, GPIO_PIN_SET);

	  /*Configure GPIO pins : D2_Pin D3_Pin D4_Pin D5_Pin
	                           D6_Pin D7_Pin D0_Pin D1_Pin */
	  GPIO_InitStruct.Pin = D2_Pin|D3_Pin|D4_Pin|D5_Pin
	                          |D6_Pin|D7_Pin|D0_Pin|D1_Pin;
	  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	  GPIO_InitStruct.Pull = GPIO_NOPULL;
	  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	  /*Configure GPIO pins : A16_Pin A17_Pin */
	  GPIO_InitStruct.Pin = A16_Pin|A17_Pin;
	  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	  GPIO_InitStruct.Pull = GPIO_NOPULL;
	  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	  /*Configure GPIO pins : A2_Pin A3_Pin A4_Pin A5_Pin
	                           A6_Pin A7_Pin A8_Pin A9_Pin
	                           A10_Pin A11_Pin A12_Pin A13_Pin
	                           A14_Pin A15_Pin A0_Pin A1_Pin */
	  GPIO_InitStruct.Pin = A2_Pin|A3_Pin|A4_Pin|A5_Pin
	                          |A6_Pin|A7_Pin|A8_Pin|A9_Pin
	                          |A10_Pin|A11_Pin|A12_Pin|A13_Pin
	                          |A14_Pin|A15_Pin|A0_Pin|A1_Pin;
	  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	  GPIO_InitStruct.Pull = GPIO_NOPULL;
	  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
}

void initROMRead(){
	  GPIO_InitTypeDef GPIO_InitStruct = {0};

	  HAL_GPIO_WritePin(OE_GPIO_Port, OE_Pin, GPIO_PIN_SET);
	  HAL_GPIO_WritePin(WE_GPIO_Port, WE_Pin, GPIO_PIN_SET);

	  /*Configure GPIO pins : D2_Pin D3_Pin D4_Pin D5_Pin
	                           D6_Pin D7_Pin D0_Pin D1_Pin */
	  GPIO_InitStruct.Pin = D2_Pin|D3_Pin|D4_Pin|D5_Pin
	                          |D6_Pin|D7_Pin|D0_Pin|D1_Pin;
	  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	  GPIO_InitStruct.Pull = GPIO_NOPULL;
	  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	  /*Configure GPIO pins : A16_Pin A17_Pin */
	  GPIO_InitStruct.Pin = A16_Pin|A17_Pin;
	  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	  GPIO_InitStruct.Pull = GPIO_NOPULL;
	  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
	  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	  /*Configure GPIO pins : A2_Pin A3_Pin A4_Pin A5_Pin
	                           A6_Pin A7_Pin A8_Pin A9_Pin
	                           A10_Pin A11_Pin A12_Pin A13_Pin
	                           A14_Pin A15_Pin A0_Pin A1_Pin */
	  GPIO_InitStruct.Pin = A2_Pin|A3_Pin|A4_Pin|A5_Pin
	                          |A6_Pin|A7_Pin|A8_Pin|A9_Pin
	                          |A10_Pin|A11_Pin|A12_Pin|A13_Pin
	                          |A14_Pin|A15_Pin|A0_Pin|A1_Pin;
	  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	  GPIO_InitStruct.Pull = GPIO_NOPULL;
	  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
	  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
}

void initROMWrite(){
	  GPIO_InitTypeDef GPIO_InitStruct = {0};

	  HAL_GPIO_WritePin(OE_GPIO_Port, OE_Pin, GPIO_PIN_SET);
	  HAL_GPIO_WritePin(WE_GPIO_Port, WE_Pin, GPIO_PIN_SET);

	  /*Configure GPIO pins : A16_Pin A17_Pin */
	  GPIO_InitStruct.Pin = A16_Pin|A17_Pin;
	  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	  GPIO_InitStruct.Pull = GPIO_NOPULL;
	  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
	  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	  /*Configure GPIO pins : A2_Pin A3_Pin A4_Pin A5_Pin
	                           A6_Pin A7_Pin A8_Pin A9_Pin
	                           A10_Pin A11_Pin A12_Pin A13_Pin
	                           A14_Pin A15_Pin A0_Pin A1_Pin */
	  GPIO_InitStruct.Pin = A2_Pin|A3_Pin|A4_Pin|A5_Pin
	                          |A6_Pin|A7_Pin|A8_Pin|A9_Pin
	                          |A10_Pin|A11_Pin|A12_Pin|A13_Pin
	                          |A14_Pin|A15_Pin|A0_Pin|A1_Pin;
	  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	  GPIO_InitStruct.Pull = GPIO_NOPULL;
	  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
	  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);


	  /*Configure GPIO pins : D2_Pin D3_Pin D4_Pin D5_Pin
	                           D6_Pin D7_Pin D0_Pin D1_Pin */
	  GPIO_InitStruct.Pin = D2_Pin|D3_Pin|D4_Pin|D5_Pin
	                          |D6_Pin|D7_Pin|D0_Pin|D1_Pin;
	  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	  GPIO_InitStruct.Pull = GPIO_NOPULL;
	  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
	  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

void setAddrBus(uint32_t addr){
	HAL_GPIO_WritePin(A17_GPIO_Port, A17_Pin, GETBIT(addr,17));
	HAL_GPIO_WritePin(A16_GPIO_Port, A16_Pin, GETBIT(addr,16));
	HAL_GPIO_WritePin(A15_GPIO_Port, A15_Pin, GETBIT(addr,15));
	HAL_GPIO_WritePin(A14_GPIO_Port, A14_Pin, GETBIT(addr,14));
	HAL_GPIO_WritePin(A13_GPIO_Port, A13_Pin, GETBIT(addr,13));
	HAL_GPIO_WritePin(A12_GPIO_Port, A12_Pin, GETBIT(addr,12));
	HAL_GPIO_WritePin(A11_GPIO_Port, A11_Pin, GETBIT(addr,11));
	HAL_GPIO_WritePin(A10_GPIO_Port, A10_Pin, GETBIT(addr,10));
	HAL_GPIO_WritePin( A9_GPIO_Port,  A9_Pin, GETBIT(addr, 9));
	HAL_GPIO_WritePin( A8_GPIO_Port,  A8_Pin, GETBIT(addr, 8));
	HAL_GPIO_WritePin( A7_GPIO_Port,  A7_Pin, GETBIT(addr, 7));
	HAL_GPIO_WritePin( A6_GPIO_Port,  A6_Pin, GETBIT(addr, 6));
	HAL_GPIO_WritePin( A5_GPIO_Port,  A5_Pin, GETBIT(addr, 5));
	HAL_GPIO_WritePin( A4_GPIO_Port,  A4_Pin, GETBIT(addr, 4));
	HAL_GPIO_WritePin( A3_GPIO_Port,  A3_Pin, GETBIT(addr, 3));
	HAL_GPIO_WritePin( A2_GPIO_Port,  A2_Pin, GETBIT(addr, 2));
	HAL_GPIO_WritePin( A1_GPIO_Port,  A1_Pin, GETBIT(addr, 1));
	HAL_GPIO_WritePin( A0_GPIO_Port,  A0_Pin, GETBIT(addr, 0));
}

void setDataBus(uint8_t data){
	HAL_GPIO_WritePin(D7_GPIO_Port, D7_Pin, GETBIT(data,7));
	HAL_GPIO_WritePin(D6_GPIO_Port, D6_Pin, GETBIT(data,6));
	HAL_GPIO_WritePin(D5_GPIO_Port, D5_Pin, GETBIT(data,5));
	HAL_GPIO_WritePin(D4_GPIO_Port, D4_Pin, GETBIT(data,4));
	HAL_GPIO_WritePin(D3_GPIO_Port, D3_Pin, GETBIT(data,3));
	HAL_GPIO_WritePin(D2_GPIO_Port, D2_Pin, GETBIT(data,2));
	HAL_GPIO_WritePin(D1_GPIO_Port, D1_Pin, GETBIT(data,1));
	HAL_GPIO_WritePin(D0_GPIO_Port, D0_Pin, GETBIT(data,0));
}

uint8_t readDataBus(){
	uint8_t temp = 0;
	temp |= (HAL_GPIO_ReadPin(D7_GPIO_Port, D7_Pin) << 7);
	temp |= (HAL_GPIO_ReadPin(D6_GPIO_Port, D6_Pin) << 6);
	temp |= (HAL_GPIO_ReadPin(D5_GPIO_Port, D5_Pin) << 5);
	temp |= (HAL_GPIO_ReadPin(D4_GPIO_Port, D4_Pin) << 4);
	temp |= (HAL_GPIO_ReadPin(D3_GPIO_Port, D3_Pin) << 3);
	temp |= (HAL_GPIO_ReadPin(D2_GPIO_Port, D2_Pin) << 2);
	temp |= (HAL_GPIO_ReadPin(D1_GPIO_Port, D1_Pin) << 1);
	temp |= (HAL_GPIO_ReadPin(D0_GPIO_Port, D0_Pin) << 0);
	return temp;
}

uint8_t wait(uint32_t x){
	volatile uint32_t y;
	y=x;
	while(y--);
	return((uint8_t)y);
}

uint8_t LLReadROM(uint32_t addr){
	volatile uint8_t temp;
	setAddrBus(addr);
	temp = wait(T);
	HAL_GPIO_WritePin(OE_GPIO_Port, OE_Pin, GPIO_PIN_RESET);
	temp = wait(T);
	temp = readDataBus();
	HAL_GPIO_WritePin(OE_GPIO_Port, OE_Pin, GPIO_PIN_SET);
	return(temp);
}

uint8_t readROM(uint32_t addr){
	initROMProgramming();
	setDataBusModeInput();
	return LLReadROMFast(addr);
}

void LLWriteROM(uint32_t addr, uint8_t data){
	volatile uint8_t temp;
	setAddrBus(addr);
	setDataBus(data);
	HAL_GPIO_WritePin(WE_GPIO_Port, WE_Pin, GPIO_PIN_RESET);
	temp = wait(T);
	HAL_GPIO_WritePin(WE_GPIO_Port, WE_Pin, GPIO_PIN_SET);
	wait(temp);
}

uint8_t ToggleBitWait(uint32_t addr){
	uint8_t first, second;
	//initROMRead();
	first  = LLReadROM(addr) & 1<<6;
	second = LLReadROM(addr) & 1<<6;
	return (first != second);
}

uint8_t writeROM(uint32_t addr, uint8_t data){
	initROMWrite();
	// Step 1
	LLWriteROM(0x5555, 0xAA);
	LLWriteROM(0x2AAA, 0x55);
	LLWriteROM(0x5555, 0xA0);

	// Step 2
	LLWriteROM(addr, data);

	// Step 3

	initROMRead();
	do{
		do{} while (ToggleBitWait(addr));
	} while (ToggleBitWait(addr));

	return 0;
}

uint8_t eraseROM(){
	uint32_t addr = 10;
	initROMWrite();
	// Step 1
	LLWriteROM(0x5555, 0xAA);
	LLWriteROM(0x2AAA, 0x55);
	LLWriteROM(0x5555, 0x80);
	LLWriteROM(0x5555, 0xAA);
	LLWriteROM(0x2AAA, 0x55);
	LLWriteROM(0x5555, 0x10);

	// Step 2

	initROMRead();
	do{
		do{} while (ToggleBitWait(addr));
	} while (ToggleBitWait(addr));

	return 0;
}

/* Fast ROM port operations using direct register access */

void setAddrBusFast(uint32_t addr){
	GPIOE->ODR = (uint16_t)(addr & 0xFFFF);
	HAL_GPIO_WritePin(A16_GPIO_Port, A16_Pin, (addr >> 16) & 1);
	HAL_GPIO_WritePin(A17_GPIO_Port, A17_Pin, (addr >> 17) & 1);
}

void setDataBusFast(uint8_t data){
	GPIOB->ODR = (GPIOB->ODR & 0x00FF) | ((uint16_t)data << 8);
}

uint8_t readDataBusFast(void){
	return (uint8_t)((GPIOB->IDR >> 8) & 0xFF);
}

void setDataBusModeInput(void){
	GPIOB->MODER = (GPIOB->MODER & 0x0000FFFF); // input mode for PB8..PB15
}

void setDataBusModeOutput(void){
	GPIOB->MODER = (GPIOB->MODER & 0x0000FFFF) | 0x55550000; // output mode for PB8..PB15
}

void LLWriteROMFast(uint32_t addr, uint8_t data){
	setAddrBusFast(addr);
	setDataBusFast(data);
	HAL_GPIO_WritePin(WE_GPIO_Port, WE_Pin, GPIO_PIN_RESET);
	wait(T);
	HAL_GPIO_WritePin(WE_GPIO_Port, WE_Pin, GPIO_PIN_SET);
	wait(T);
}

uint8_t LLReadROMFast(uint32_t addr){
	volatile uint8_t temp;
	setAddrBusFast(addr);
	wait(T);
	HAL_GPIO_WritePin(OE_GPIO_Port, OE_Pin, GPIO_PIN_RESET);
	wait(T);
	temp = readDataBusFast();
	HAL_GPIO_WritePin(OE_GPIO_Port, OE_Pin, GPIO_PIN_SET);
	return temp;
}

uint8_t ToggleBitWaitFast(uint32_t addr){
	uint8_t first, second;
	first  = LLReadROMFast(addr) & (1 << 6);
	second = LLReadROMFast(addr) & (1 << 6);
	return (first != second);
}

uint8_t writeROMFast(uint32_t addr, uint8_t data){
	setDataBusModeOutput();
	LLWriteROMFast(0x5555, 0xAA);
	LLWriteROMFast(0x2AAA, 0x55);
	LLWriteROMFast(0x5555, 0xA0);
	LLWriteROMFast(addr, data);

	setDataBusModeInput();
	uint32_t timeout = 50000;
	do{
		do{} while (ToggleBitWaitFast(addr) && --timeout);
	} while (ToggleBitWaitFast(addr) && --timeout);

	return 0;
}

uint8_t eraseROMFast(void){
	uint32_t addr = 10;
	setDataBusModeOutput();
	LLWriteROMFast(0x5555, 0xAA);
	LLWriteROMFast(0x2AAA, 0x55);
	LLWriteROMFast(0x5555, 0x80);
	LLWriteROMFast(0x5555, 0xAA);
	LLWriteROMFast(0x2AAA, 0x55);
	LLWriteROMFast(0x5555, 0x10);

	setDataBusModeInput();
	uint32_t timeout = 5000000;
	do{
		do{} while (ToggleBitWaitFast(addr) && --timeout);
	} while (ToggleBitWaitFast(addr) && --timeout);

	return 0;
}

void initROMProgramming(void){
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	HAL_GPIO_WritePin(OE_GPIO_Port, OE_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(WE_GPIO_Port, WE_Pin, GPIO_PIN_SET);

	// Configure A16, A17 as OUTPUT PP
	GPIO_InitStruct.Pin = A16_Pin | A17_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	// Configure A0..A15 as OUTPUT PP
	GPIO_InitStruct.Pin = GPIO_PIN_All;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
	HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

	setDataBusModeOutput();
}

const char DEFAULT_BRAINFUINO_LOGO_BF[] = 
	"++++++++++[>+++++++++>++++++++++++>++++++>+++++>++++++++>+++++++++++>++++<<<<<<<-]"
	">+>+++>>>>>--------..<<<<<<++++....>>>>>>............<<<<<<.>>>>>>........<<<<<<.."
	">>>>>>.......<<<<<<.>>>>>>.............<<<<<<<+++++++++++++.---.>>>>>>>.<<<<<+.>>>>>."
	"<<<<<<..>>>>>>.+++++++++.---------.<<<<<<.>>>>>>.<<<<<<..>>>>>>.<<<<<<..>>>>>>.<<<<<<."
	">>>>>>++++++++.<<<<<<.>>>>>>+.<<<<<<.>>>>>>---------.<<<<<<..>>>>>>..+++++++++++++++."
	"---------------.<<<<<<.>.<.>>>>>>...<<<<<<.>>>>>>++++++++.<<<<<<.>>>>>>+.<<<<<<."
	">>>>>>---------.<<<<<<..>>>>>>...<<<<<<...>>>>>>..<<<<<<<+++.---.>>>>>>>.<<<<<.>>>>>.."
	"<<<<<<.>>>>>>.<<<<<<---.>.>>>>>.+++++++.<<<<<<+++..>>>>>>++++++++.---------------."
	"<<<<<<.+.>>>>>>.<<<<<.>>>>>.<<<<<.>>>>>.+++++++.<<<<<<-.>>>>>>-------.<<<<<<---.>.>>>>>."
	"<<<<<.<+++.>.>>>>>.<<<<<.>>>>>.<<<<<.>>>>>.<<<<<.>>>>>.<<<<<.>>>>>.+++++++.<<<<<<."
	">>>>>>-------.<<<<<<---.>>>>>>.+++++++++++++++.---------------.<<<<<<+++.>>>>>>.<<<<<<---."
	">>>>>>.<<<<<<<+++.---.>>>>>>>.<<<<<.>>>>>.<<<<<.<+++.>>>>>>+++++++++.---------.<<<<<."
	">>>>>.<<<<<.>>>>>.<<<<<.>>>>>.++++++++.<<<<<<.>.>>>>>--------.<<<<<.>>>>>.<<<<<.>>>>>."
	"<<<<<.>>>>>.<<<<<.>>>>>.<<<<<.>>>>>..<<<<<<.>.>>>>>.<<<<<.<.>.>>>>>.<<<<<.>>>>>.<<<<<."
	">>>>>.<<<<<.>>>>>.<<<<<.>>>>>.<<<<<.>>>>>.++++++++.<<<<<<.>>>>>>+.---------.<<<<<.<<+++."
	"---.>>>>>>>.<<<<<.<....>>>>>>+++++++++++++++.<<<<<.<.>.>>>>>---------------..<<<<<<---."
	"+++..>>>>>>++++++++++++.<<<<<<.>.<.>.<.>.>>>>>------------.<<<<<.<.>.<.>.>>>>>..<<<<<<---."
	"+++..>>>>>>++++++++++++.<<<<<<.>.<.>.<.>.>>>>>------------.<<<<<.<.>.<---.+++...>>>>>>"
	"+++++++++++++++.---------------.<<<<<<<+++.---.>>>>>>>.................................................."
	"<<<<<<<+++.---."
	"[-]+[]";

uint8_t flashBufferToROM(const uint8_t *code, uint32_t len, const char *title, uint8_t append_loop, uint8_t auto_launch){
	if (title){
		CDC_Printf("\r\nFlashing %s (%lu bytes)...\r\n", title, len);
	} else {
		CDC_Printf("\r\nData size: %lu bytes\r\n", len);
	}

	// Hold FPGA in reset and turn on red LED during flash
	HAL_GPIO_WritePin(BF_RST_GPIO_Port, BF_RST_Pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);

	CDC_Print("Erasing ROM... ");
	initROMProgramming();
	eraseROMFast();
	CDC_Print("Done.\r\n");

	// Total length including optional robust endless loop ([-]+[])
	uint32_t total_len = len;
	const char loop_str[] = "[-]+[]";
	if (append_loop && (total_len + 6 <= FLASH_CAPACITY)){
		total_len += 6;
	}

	// Write buffer to Flash with percentage updates
	uint32_t step = (total_len >= 10) ? (total_len / 10) : 1;
	for (uint32_t i = 0; i < total_len; i++){
		uint8_t byte_to_write = (i < len) ? code[i] : (uint8_t)loop_str[i - len];
		writeROMFast(i, byte_to_write);
		if (((i + 1) % step == 0) || ((i + 1) == total_len)){
			uint32_t pct = ((i + 1) * 100) / total_len;
			CDC_Printf("Writing: %lu%% (%lu / %lu bytes)...\r\n", pct, (i + 1), total_len);
		}
	}

	// Verify Flash contents against source buffer
	CDC_Print("Verifying ROM... ");
	uint8_t verify_ok = 1;
	for (uint32_t i = 0; i < total_len; i++){
		uint8_t expected = (i < len) ? code[i] : (uint8_t)loop_str[i - len];
		uint8_t val = LLReadROMFast(i);
		if (val != expected){
			CDC_Printf("MISMATCH at byte %lu! (Expected 0x%02X, read 0x%02X)\r\n", i, expected, val);
			verify_ok = 0;
			break;
		}
	}
	if (verify_ok){
		CDC_Print("OK!\r\n");
	}

	CDC_Print("Finished! Program written successfully.\r\n");

	if (auto_launch){
		CDC_Print("Auto-launching program...\r\n");
		HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
		initROMNormal();
		wait(1000);
		HAL_GPIO_WritePin(OE_GPIO_Port, OE_Pin, GPIO_PIN_RESET);
		wait(1000);
		HAL_GPIO_WritePin(BF_RST_GPIO_Port, BF_RST_Pin, GPIO_PIN_RESET);
		HAL_Delay(10);
		HAL_GPIO_WritePin(BF_RST_GPIO_Port, BF_RST_Pin, GPIO_PIN_SET);
		state = STATE_RUN;
	} else {
		CDC_Print("Press the Reset button to run the program.\r\n");
	}

	return verify_ok;
}

void flashDefaultLogoProgram(void){
	CDC_Print("\r\n\r\n=== RESTORING DEFAULT BRAINFUINO DEMO ===\r\n");
	flashBufferToROM((const uint8_t *)DEFAULT_BRAINFUINO_LOGO_BF,
	                 sizeof(DEFAULT_BRAINFUINO_LOGO_BF) - 1,
	                 "Brainfuino ASCII Logo",
	                 0,
	                 1);

	// Reset any active programming state
	prog_active = 0;
	prog_rx_count = 0;
	prog_total_received = 0;
	prog_flash_addr = 0;
	prog_is_streaming = 0;
	staging_head = 0;
	staging_tail = 0;
	stream_init_done = 0;
}

void Execute_DFU_Jump(void){
	CDC_Print("\r\n[Rebooting into STM32 USB DFU Bootloader...]\r\n");

	// Wait for CDC output to flush to host
	uint32_t timeout = 200000;
	while (TxBusy() && --timeout);
	for (volatile int i = 0; i < 500000; i++);

	// De-initialize USB peripheral and stack
	USBD_Stop(&hUsbDeviceFS);
	USBD_DeInit(&hUsbDeviceFS);

	// Physically drive USB DP (PA12) LOW for 200 ms to trigger host disconnect
	__HAL_RCC_GPIOA_CLK_ENABLE();
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = GPIO_PIN_12;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_RESET);
	for (volatile int i = 0; i < 1000000; i++);

	// Set magic flag in SRAM (survives NVIC_SystemReset)
	DFU_MAGIC_ADDR = DFU_MAGIC_VALUE;

	// Trigger genuine hardware system reset into ST ROM bootloader
	NVIC_SystemReset();
}

void writeBFInput(uint8_t data){
	HAL_GPIO_WritePin(BF_IN7_GPIO_Port, BF_IN7_Pin, GETBIT(data,7));
	HAL_GPIO_WritePin(BF_IN6_GPIO_Port, BF_IN6_Pin, GETBIT(data,6));
	HAL_GPIO_WritePin(BF_IN5_GPIO_Port, BF_IN5_Pin, GETBIT(data,5));
	HAL_GPIO_WritePin(BF_IN4_GPIO_Port, BF_IN4_Pin, GETBIT(data,4));
	HAL_GPIO_WritePin(BF_IN3_GPIO_Port, BF_IN3_Pin, GETBIT(data,3));
	HAL_GPIO_WritePin(BF_IN2_GPIO_Port, BF_IN2_Pin, GETBIT(data,2));
	HAL_GPIO_WritePin(BF_IN1_GPIO_Port, BF_IN1_Pin, GETBIT(data,1));
	HAL_GPIO_WritePin(BF_IN0_GPIO_Port, BF_IN0_Pin, GETBIT(data,0));
}

uint8_t readBFOutput(){/*
	uint8_t temp = 0;
	temp |= (HAL_GPIO_ReadPin(BF_OUT7_GPIO_Port, BF_OUT7_Pin) << 7);
	temp |= (HAL_GPIO_ReadPin(BF_OUT6_GPIO_Port, BF_OUT6_Pin) << 6);
	temp |= (HAL_GPIO_ReadPin(BF_OUT5_GPIO_Port, BF_OUT5_Pin) << 5);
	temp |= (HAL_GPIO_ReadPin(BF_OUT4_GPIO_Port, BF_OUT4_Pin) << 4);
	temp |= (HAL_GPIO_ReadPin(BF_OUT3_GPIO_Port, BF_OUT3_Pin) << 3);
	temp |= (HAL_GPIO_ReadPin(BF_OUT2_GPIO_Port, BF_OUT2_Pin) << 2);
	temp |= (HAL_GPIO_ReadPin(BF_OUT1_GPIO_Port, BF_OUT1_Pin) << 1);
	temp |= (HAL_GPIO_ReadPin(BF_OUT0_GPIO_Port, BF_OUT0_Pin) << 0);
	return temp;*/
	return (uint8_t)GPIOB->IDR;
}



void OLDECDC_Receive_Callback(uint8_t *buff, uint32_t len){
	static uint8_t prev, temp;
	switch(*buff){
		case 'g':
				CDC_Transmit_FS((uint8_t *)"Gato", 4U);
				HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
				break;
		case 'c':
				//CDC_Transmit_FS((uint8_t *)"\nErasing ROM", 12U);
				if(!eraseROM())
					CDC_Transmit_FS((uint8_t *)" ... Done!\n", 10U);
				else
					CDC_Transmit_FS((uint8_t *)" ... Nope!\n", 10U);
				break;
		case 'w':
				//CDC_Transmit_FS((uint8_t *)"\nWriting ", 9U);
				//CDC_Transmit_FS(&prev, 1U);
				if(!writeROM(10U,prev))
					CDC_Transmit_FS((uint8_t *)" ... Done!\n", 10U);
				else
					CDC_Transmit_FS((uint8_t *)" ... Nope!\n", 10U);
				break;
		case 'r':
				//CDC_Transmit_FS((uint8_t *)"\nReading ... ", 13U);
				temp = readROM(10U);
				CDC_Transmit_FS(&temp, 1U);
				//CDC_Transmit_FS((uint8_t *)"\n", 1U);
				break;
		case '0':
				temp = readROM(0U);
				CDC_Transmit_FS(&temp, 1U);
				break;
		case '1':
				temp = readROM(1U);
				CDC_Transmit_FS(&temp, 1U);
				break;
		case '2':
				temp = readROM(2U);
				CDC_Transmit_FS(&temp, 1U);
				break;
		case '3':
				temp = readROM(3U);
				CDC_Transmit_FS(&temp, 1U);
				break;
		case '4':
				temp = readROM(4U);
				CDC_Transmit_FS(&temp, 1U);
				break;
		case '5':
				temp = readROM(5U);
				CDC_Transmit_FS(&temp, 1U);
				break;
		case '@':
				writeROM(0U,',');
				writeROM(1U,'[');
				writeROM(2U,'+');
				writeROM(3U,'.');
				writeROM(4U,',');
				if(!writeROM(5U,']'))
					CDC_Transmit_FS((uint8_t *)" ... Done!\n", 10U);
				break;

		default:
				CDC_Transmit_FS(buff, len);
				HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
				break;
	}

	prev=*buff;
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */

  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
