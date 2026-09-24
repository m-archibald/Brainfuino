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
#define STATE_CONFIG    2
#define STATE_LIB_ADD   3

#define MENU_PAGE_MAIN      0
#define MENU_PAGE_SETTINGS  1
#define MENU_PAGE_LIBRARY   2

#define LIB_ADD_NAME            0
#define LIB_ADD_PASTE_PROMPT    1
#define LIB_ADD_PASTE           2
#define LIB_ADD_VERIFY_PROMPT   3
#define LIB_ADD_VERIFYING       4
#define LIB_ADD_COMMIT          5
#define LIB_ADD_CAPACITY_WARN   6

#define LIBRARY_TOC_ADDR        0x08014000UL
#define LIBRARY_POOL_ADDR       0x08014800UL
#define LIBRARY_POOL_SIZE       (44UL * 1024UL) // 44 kB (Pages 41-62, safe from 80 kB firmware region)
#define MAX_LIB_SLOTS           64
#define LIB_NAME_MAX_LEN        16

typedef struct {
	uint8_t  status;        // 0xFF = Empty, 0x01 = Active, 0x00 = Deleted
	char     name[LIB_NAME_MAX_LEN]; // 16 bytes: Null-terminated string (max 15 chars)
	uint8_t  pruned;        // 1 if non-BF comments stripped
	uint8_t  pad[2];        // 2 bytes padding
	uint32_t flash_offset;  // Byte offset from LIBRARY_POOL_ADDR
	uint32_t size;          // Payload size in bytes
	uint32_t crc32;         // Additive checksum / CRC
} ProgramDescriptor;

typedef union {
	ProgramDescriptor desc;
	uint32_t words[sizeof(ProgramDescriptor) / 4];
} ProgramSlot;

#define OUTBOX_CAPACITY 1024

#define PROG_BUF_SIZE           4096
#define STAGING_BUF_SIZE        1024
#define FLASH_CAPACITY          262144
#define PROG_IDLE_TIMEOUT_MS    100
#define STREAM_REPORT_INTERVAL  1024

// Run Mode speed hotkey options
#define SPEED_HOTKEY_NONE        0   // Disabled
#define SPEED_HOTKEY_PGUP_PGDN   1   // Page Up / Page Down (\x1b[5~ / \x1b[6~)
#define SPEED_HOTKEY_UP_DOWN     2   // Up Arrow / Down Arrow (\x1b[A / \x1b[B)
#define SPEED_HOTKEY_PLUS_MINUS  3   // '+' / '-'

// Manual step tick advance options
#define STEP_TICKS_1             0   // 1 Tick
#define STEP_TICKS_10            1   // 10 Ticks
#define STEP_TICKS_100           2   // 100 Ticks
#define STEP_TICKS_1K            3   // 1,000 Ticks
#define STEP_TICKS_10K           4   // 10,000 Ticks
#define STEP_TICKS_100K          5   // 100,000 Ticks

// Manual step key options
#define STEP_KEY_SPACE           0   // Spacebar (0x20)
#define STEP_KEY_TAB             1   // Tab (0x09)
#define STEP_KEY_ENTER           2   // Enter (0x0D / 0x0A)

#define MENU_ITEM_COUNT_COLLAPSED 9
#define MENU_ITEM_COUNT_EXPANDED  11

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
TIM_HandleTypeDef htim1;

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
volatile uint32_t head;
uint32_t leaving;
volatile uint32_t tail;
uint32_t tail_temp;
volatile uint32_t active_mco_cfg = 0;
volatile uint8_t  mco_throttled = 0;
volatile uint8_t  active_is_tim1 = 0;

// Runtime configuration settings (modifiable in interactive configuration menu)
uint8_t  cfg_auto_prog_run_mode   = 1;   // 1 = Auto-enter program mode if code paste detected in RUN mode
uint32_t cfg_auto_prog_min_bytes  = 16;  // Minimum packet size to trigger upload (safe against 3-5 byte ANSI escape keys)
uint8_t  cfg_auto_reset_after_pgm = 1;   // 1 = Automatically reset and launch program after write finishes
uint8_t  cfg_append_endless_loop  = 1;   // 1 = Automatically append '[-]+[]' infinite loop at EOF to halt PC cleanly
uint8_t  cfg_speed_hotkey_mode    = SPEED_HOTKEY_PGUP_PGDN;
uint8_t  cfg_manual_step_enabled  = 0;   // 0 = Normal continuous clock, 1 = Manual step mode (clock paused)
uint8_t  cfg_manual_step_ticks    = STEP_TICKS_100; // Step burst size (1, 10, 100, 1k, 10k, 100k)
uint8_t  cfg_manual_step_key      = STEP_KEY_SPACE; // Step trigger key (Space, Tab, Enter)
uint8_t  cfg_prune_on_paste       = 0;   // 0 = Disabled, 1 = Strip non-BF commands on paste
uint8_t  cfg_prune_in_library     = 1;   // 0 = Disabled, 1 = Strip non-BF commands when saving to library
uint8_t  cfg_cr_to_lf             = 1;   // 1 = Convert Windows \r (CR) -> \n (LF) in RUN mode

#define SERIAL_INBOX_SIZE 64
static volatile uint8_t serial_inbox[SERIAL_INBOX_SIZE];
static volatile uint8_t serial_inbox_head = 0;
static volatile uint8_t serial_inbox_tail = 0;

// Manual clock stepping accumulator queue
volatile uint32_t manual_step_ticks_pending = 0;

// Delayed Flash wear-leveling timer
uint8_t  settings_dirty = 0;
uint32_t settings_dirty_tick = 0;

uint8_t  menu_page = MENU_PAGE_MAIN;
int8_t   menu_cursor = 0;
volatile uint8_t menu_needs_render = 0;
volatile uint8_t menu_exit_requested = 0;
volatile int8_t  menu_action_pending = -1;
volatile int8_t  menu_action_dir = 1; // +1 = Right/Enter/Space, -1 = Left

// Library Add Program state machine
volatile uint8_t lib_add_substate = LIB_ADD_NAME;
char     lib_name_buf[LIB_NAME_MAX_LEN];
uint8_t  lib_name_len = 0;
uint32_t lib_code_size = 0;
uint32_t lib_raw_rx_size = 0;
uint32_t lib_add_last_rx_tick = 0;
uint32_t lib_verify_start_tick = 0;
uint8_t  lib_verify_requested = 0;
volatile uint8_t lib_start_add_requested = 0;
volatile int8_t  lib_delete_pending_slot = -1;
volatile int8_t  lib_load_pending_slot = -1;
volatile int8_t  lib_dump_pending_slot = -1;
volatile uint8_t lib_dump_viewing = 0;
volatile uint8_t lib_verify_choice_pending = 0;
volatile uint8_t lib_verify_choice = 0;
volatile uint8_t lib_verify_early_commit = 0;
volatile int8_t  lib_capacity_delete_slot = -1;

volatile uint8_t dfu_requested;
volatile uint8_t reset_requested;
volatile uint8_t dynamic_schema_requested = 0;
volatile uint8_t dynamic_lib_requested = 0;
volatile int8_t  cmd_lib_run_slot = -1;
volatile int8_t  cmd_lib_dump_slot = -1;
volatile int8_t  cmd_lib_del_slot = -1;
volatile uint8_t lib_auto_verify = 0;
volatile uint8_t cmd_restore_requested = 0;

// Button debounce and hold tracking
uint8_t  btn_is_pressed = 0;
uint8_t  btn_last_sample = 1;
uint32_t btn_last_stable_tick = 0;
uint32_t btn_press_tick = 0;
uint8_t  btn_held_1s = 0;
uint8_t  btn_held_3s = 0;
uint8_t  btn_held_8s = 0;

// Dedicated program mode buffers and state
uint8_t prog_buffer[PROG_BUF_SIZE];
volatile uint8_t stream_staging[STAGING_BUF_SIZE];
volatile uint16_t staging_head;
volatile uint16_t staging_tail;
volatile uint32_t prog_rx_count;
volatile uint32_t prog_total_received;
volatile uint32_t prog_raw_received;
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
static void MX_TIM1_Init(void);
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
void menu_enter(void);
void menu_exit(void);
void menu_render(void);
void menu_execute_action(uint8_t item, int8_t dir);
void led_update_pulse(uint32_t period_ms);
void settings_load(void);
void settings_save(void);

// Library prototypes
void lib_init_toc(void);
void lib_refresh_display_list(void);
uint8_t lib_get_active_count(void);
uint32_t lib_get_total_used_bytes(void);
int8_t lib_find_free_slot(void);
uint32_t lib_calculate_next_pool_offset(void);
void lib_rewrite_toc_compact(void);
uint8_t lib_save_program(const char *name, uint32_t size, uint8_t pruned);
void lib_delete_program(uint8_t slot);
void lib_load_and_run(uint8_t slot);
void lib_dump_program(uint8_t slot);
void lib_export_dynamic_schema(void);
void lib_start_add_program(void);
void CDC_Write(const uint8_t *data, uint32_t len);


/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin){
	if (GPIO_Pin == GPIO_PIN_1){  // InStrobe falling edge: FPGA acknowledged current input byte
		HAL_GPIO_WritePin(BF_INCMG_GPIO_Port, BF_INCMG_Pin, GPIO_PIN_RESET);
		if (serial_inbox_head != serial_inbox_tail){
			uint8_t next_b = serial_inbox[serial_inbox_head];
			serial_inbox_head = (serial_inbox_head + 1) % SERIAL_INBOX_SIZE;
			writeBFInput(next_b);
			HAL_GPIO_WritePin(BF_INCMG_GPIO_Port, BF_INCMG_Pin, GPIO_PIN_SET);
			serial_input = 1;
		} else {
			serial_input = 0;
		}
	}
}

typedef struct {
	const char *name;
	uint32_t freq_hz;
	uint8_t is_tim1;
	uint16_t tim1_psc;
	uint16_t tim1_arr;
	uint32_t mco_cfg;
} FreqConfig;

static const FreqConfig freq_table[] = {
	// Ultra-low frequencies (TIM1_CH1 PWM on PA8)
	{ "10 Hz",          10, 1, 47999, 99,   0 },
	{ "25 Hz",          25, 1, 47999, 39,   0 },
	{ "50 Hz",          50, 1, 47999, 19,   0 },
	{ "100 Hz",        100, 1, 47999, 9,    0 },
	{ "250 Hz",        250, 1, 47999, 3,    0 },
	{ "500 Hz",        500, 1, 47999, 1,    0 },
	{ "1 kHz",        1000, 1, 479,   99,   0 },
	{ "2 kHz",        2000, 1, 479,   49,   0 },
	{ "5 kHz",        5000, 1, 479,   19,   0 },
	{ "10 kHz",      10000, 1, 479,   9,    0 },
	{ "25 kHz",      25000, 1, 479,   3,    0 },
	{ "50 kHz",      50000, 1, 479,   1,    0 },

	// Standard & high frequencies (MCO on PA8)
	{ "62.5 kHz",    62500, 0, 0,     0,    RCC_MCO1SOURCE_HSI   | RCC_MCODIV_128 },
	{ "125 kHz",    125000, 0, 0,     0,    RCC_MCO1SOURCE_HSI   | RCC_MCODIV_64  },
	{ "250 kHz",    250000, 0, 0,     0,    RCC_MCO1SOURCE_HSI   | RCC_MCODIV_32  },
	{ "500 kHz",    500000, 0, 0,     0,    RCC_MCO1SOURCE_HSI   | RCC_MCODIV_16  },
	{ "750 kHz",    750000, 0, 0,     0,    RCC_MCO1SOURCE_HSI48 | RCC_MCODIV_64  },
	{ "1 MHz",     1000000, 0, 0,     0,    RCC_MCO1SOURCE_HSI   | RCC_MCODIV_8   },
	{ "1.5 MHz",   1500000, 0, 0,     0,    RCC_MCO1SOURCE_HSI48 | RCC_MCODIV_32  },
	{ "2 MHz",     2000000, 0, 0,     0,    RCC_MCO1SOURCE_HSI   | RCC_MCODIV_4   },
	{ "3 MHz",     3000000, 0, 0,     0,    RCC_MCO1SOURCE_HSI48 | RCC_MCODIV_16  },
	{ "4 MHz",     4000000, 0, 0,     0,    RCC_MCO1SOURCE_HSI   | RCC_MCODIV_2   },
	{ "6 MHz",     6000000, 0, 0,     0,    RCC_MCO1SOURCE_HSI48 | RCC_MCODIV_8   },
	{ "8 MHz",     8000000, 0, 0,     0,    RCC_MCO1SOURCE_HSI   | RCC_MCODIV_1   },
	{ "12 MHz",   12000000, 0, 0,     0,    RCC_MCO1SOURCE_HSI48 | RCC_MCODIV_4   },
};
#define FREQ_COUNT ((uint8_t)(sizeof(freq_table) / sizeof(freq_table[0])))
#define DEFAULT_FREQ_INDEX 15 // 500 kHz

void set_freq(uint8_t idx){
	if (idx >= FREQ_COUNT) idx = DEFAULT_FREQ_INDEX;
	freq = idx;

	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = BF_CLK_Pin;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;

	if (freq_table[idx].is_tim1){
		// Disable MCO output first
		HAL_RCC_MCOConfig(RCC_MCO, RCC_MCO1SOURCE_NOCLOCK, RCC_MCODIV_1);
		active_mco_cfg = 0;

		// Reconfigure PA8 as TIM1_CH1 (AF2)
		GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
		GPIO_InitStruct.Alternate = GPIO_AF2_TIM1;
		HAL_GPIO_Init(BF_CLK_GPIO_Port, &GPIO_InitStruct);

		// Configure TIM1 Prescaler, Auto-Reload, and Compare Register (50% duty)
		TIM1->CR1 &= ~TIM_CR1_CEN;
		TIM1->PSC = freq_table[idx].tim1_psc;
		TIM1->ARR = freq_table[idx].tim1_arr;
		TIM1->CCR1 = (freq_table[idx].tim1_arr + 1) / 2;
		TIM1->EGR = TIM_EGR_UG;
		active_is_tim1 = 1;

		if (!cfg_manual_step_enabled){
			TIM1->CR1 |= TIM_CR1_CEN;
		}
	} else {
		// Disable TIM1 counter
		TIM1->CR1 &= ~TIM_CR1_CEN;
		active_is_tim1 = 0;

		// Reconfigure PA8 as MCO (AF0)
		GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
		GPIO_InitStruct.Alternate = GPIO_AF0_MCO;
		HAL_GPIO_Init(BF_CLK_GPIO_Port, &GPIO_InitStruct);

		active_mco_cfg = freq_table[idx].mco_cfg;
		if (!cfg_manual_step_enabled){
			HAL_RCC_MCOConfig(RCC_MCO, active_mco_cfg & RCC_CFGR_MCO, active_mco_cfg & RCC_CFGR_MCOPRE);
		} else {
			HAL_RCC_MCOConfig(RCC_MCO, RCC_MCO1SOURCE_NOCLOCK, RCC_MCODIV_1);
		}
	}
}

#define SPEED_BADGE_WIDTH 18
static volatile uint8_t speed_overlay_remaining = 0;
void print_speed_badge(void);

const char *get_freq_name(uint8_t idx){
	if (idx >= FREQ_COUNT) return "Unknown";
	return freq_table[idx].name;
}

void speed_step_up(void){
	if (freq + 1 < FREQ_COUNT){
		set_freq(freq + 1);
		settings_dirty = 1;
		settings_dirty_tick = HAL_GetTick();
		print_speed_badge();
		HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
		for (volatile int i = 0; i < 60000; i++);
		HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
	}
}

void speed_step_down(void){
	if (freq > 0){
		set_freq(freq - 1);
		settings_dirty = 1;
		settings_dirty_tick = HAL_GetTick();
		print_speed_badge();
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

void CDC_Write(const uint8_t *data, uint32_t len){
	uint32_t offset = 0;
	while (offset < len){
		uint16_t chunk = (len - offset > 64) ? 64 : (uint16_t)(len - offset);
		uint32_t timeout = 200000;
		while (TxBusy() && --timeout);
		CDC_Transmit_FS((uint8_t *)(data + offset), chunk);
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

void print_speed_badge(void){
	char inner[24];
	char badge[32];
	snprintf(inner, sizeof(inner), "[Clock: %s]", get_freq_name(freq));
	snprintf(badge, sizeof(badge), "%-18s", inner);
	CDC_Printf("\x1b[s\x1b[2m%s\x1b[0m\x1b[u", badge);
	speed_overlay_remaining = SPEED_BADGE_WIDTH;
}

void led_update_pulse(uint32_t period_ms){
	if (period_ms == 0) return;
	uint32_t tick = HAL_GetTick();
	uint32_t phase = tick % period_ms;
	uint32_t half = period_ms / 2;
	uint32_t duty;
	if (phase < half){
		duty = (phase * 1000) / half;
	} else {
		duty = ((period_ms - phase) * 1000) / half;
	}

	// Quadratic curve for smooth, natural human eye brightness perception
	uint32_t load = SysTick->LOAD;
	if (load == 0) load = 48000;
	uint32_t threshold = ((duty * duty) / 1000) * load / 1000;

	uint32_t current_val = load - SysTick->VAL;
	if (current_val < threshold){
		LED_GPIO_Port->BSRR = LED_Pin;
	} else {
		LED_GPIO_Port->BRR = LED_Pin;
	}
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
	"<<<<<<<+++.---.";

static inline uint8_t is_bf_cmd(char c){
	return (c == '+' || c == '-' || c == '<' || c == '>' ||
	        c == '[' || c == ']' || c == '.' || c == ',');
}

// Table of Contents pointer in STM32 internal Flash (Page 24)
static volatile const ProgramDescriptor *lib_toc = (volatile const ProgramDescriptor *)LIBRARY_TOC_ADDR;

// Cached array of active slots for display in library menu
static uint8_t lib_display_slots[MAX_LIB_SLOTS];
static uint8_t lib_display_count = 0;

uint8_t lib_get_active_count(void){
	uint8_t count = 0;
	for (uint8_t s = 1; s < MAX_LIB_SLOTS; s++){
		if (lib_toc[s].status == 0x01) count++;
	}
	return count;
}

void lib_refresh_display_list(void){
	lib_display_count = 0;
	lib_display_slots[lib_display_count++] = 0; // Slot 0 is always Brainfuino Demo
	for (uint8_t s = 1; s < MAX_LIB_SLOTS; s++){
		if (lib_toc[s].status == 0x01){
			lib_display_slots[lib_display_count++] = s;
		}
	}
}

uint32_t lib_get_total_used_bytes(void){
	uint32_t total = 0;
	for (uint8_t s = 1; s < MAX_LIB_SLOTS; s++){
		if (lib_toc[s].status == 0x01){
			total += lib_toc[s].size;
		}
	}
	return total;
}

int8_t lib_find_free_slot(void){
	for (uint8_t s = 1; s < MAX_LIB_SLOTS; s++){
		if (lib_toc[s].status == 0xFF) return s;
	}
	return -1;
}

uint32_t lib_calculate_next_pool_offset(void){
	uint32_t max_end = 0;
	for (uint8_t s = 1; s < MAX_LIB_SLOTS; s++){
		if (lib_toc[s].status == 0x01){
			uint32_t end = lib_toc[s].flash_offset + lib_toc[s].size;
			end = (end + 3) & ~3; // 4-byte align
			if (end > max_end) max_end = end;
		}
	}
	return max_end;
}

#define TOC_MAGIC 0xBFC00101UL

void lib_format_toc(void){
	HAL_FLASH_Unlock();
	FLASH_EraseInitTypeDef erase;
	uint32_t err = 0;
	erase.TypeErase = FLASH_TYPEERASE_PAGES;
	erase.PageAddress = LIBRARY_TOC_ADDR;
	erase.NbPages = 1;
	__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_WRPERR | FLASH_FLAG_PGERR);
	if (HAL_FLASHEx_Erase(&erase, &err) == HAL_OK){
		ProgramSlot slot_data;
		memset(&slot_data, 0xFF, sizeof(slot_data));
		slot_data.desc.status = 0x01;
		strncpy(slot_data.desc.name, "Brainfuino Demo", LIB_NAME_MAX_LEN - 1);
		slot_data.desc.name[LIB_NAME_MAX_LEN - 1] = '\0';
		slot_data.desc.pruned = 1;
		slot_data.desc.pad[0] = 0;
		slot_data.desc.pad[1] = 0;
		slot_data.desc.flash_offset = 0;
		slot_data.desc.size = sizeof(DEFAULT_BRAINFUINO_LOGO_BF) - 1;
		slot_data.desc.crc32 = TOC_MAGIC;
		for (uint32_t w = 0; w < sizeof(ProgramDescriptor)/4; w++){
			HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, LIBRARY_TOC_ADDR + (w * 4), slot_data.words[w]);
		}
	}
	HAL_FLASH_Lock();
	lib_refresh_display_list();
}

void lib_rewrite_toc_compact(void){
	ProgramDescriptor active[MAX_LIB_SLOTS];
	uint8_t active_count = 0;
	for (uint8_t s = 1; s < MAX_LIB_SLOTS; s++){
		if (lib_toc[s].status == 0x01){
			memcpy(&active[active_count++], (const void *)&lib_toc[s], sizeof(ProgramDescriptor));
		}
	}
	HAL_FLASH_Unlock();
	FLASH_EraseInitTypeDef erase;
	uint32_t err = 0;
	erase.TypeErase = FLASH_TYPEERASE_PAGES;
	erase.PageAddress = LIBRARY_TOC_ADDR;
	erase.NbPages = 1;
	__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_WRPERR | FLASH_FLAG_PGERR);
	if (HAL_FLASHEx_Erase(&erase, &err) == HAL_OK){
		ProgramSlot slot_data;
		memset(&slot_data, 0xFF, sizeof(slot_data));
		slot_data.desc.status = 0x01;
		strncpy(slot_data.desc.name, "Brainfuino Demo", LIB_NAME_MAX_LEN - 1);
		slot_data.desc.name[LIB_NAME_MAX_LEN - 1] = '\0';
		slot_data.desc.pruned = 1;
		slot_data.desc.pad[0] = 0;
		slot_data.desc.pad[1] = 0;
		slot_data.desc.flash_offset = 0;
		slot_data.desc.size = sizeof(DEFAULT_BRAINFUINO_LOGO_BF) - 1;
		slot_data.desc.crc32 = TOC_MAGIC;
		for (uint32_t w = 0; w < sizeof(ProgramDescriptor)/4; w++){
			HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, LIBRARY_TOC_ADDR + (w * 4), slot_data.words[w]);
		}

		for (uint8_t i = 0; i < active_count; i++){
			uint32_t slot_addr = LIBRARY_TOC_ADDR + ((i + 1) * sizeof(ProgramDescriptor));
			ProgramSlot act_slot;
			memcpy(&act_slot.desc, &active[i], sizeof(ProgramDescriptor));
			for (uint32_t w = 0; w < sizeof(ProgramDescriptor)/4; w++){
				HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, slot_addr + (w * 4), act_slot.words[w]);
			}
		}
	}
	HAL_FLASH_Lock();
	lib_refresh_display_list();
}

void lib_init_toc(void){
	if (lib_toc[0].crc32 != TOC_MAGIC){
		lib_format_toc();
	} else {
		// Clean up any deleted tombstones (status != 0x01 && status != 0xFF) or gaps in slot numbering
		uint8_t needs_compact = 0;
		for (uint8_t s = 1; s < MAX_LIB_SLOTS; s++){
			if (lib_toc[s].status != 0xFF && lib_toc[s].status != 0x01){
				needs_compact = 1;
				break;
			}
		}
		uint8_t seen_empty = 0;
		for (uint8_t s = 1; s < MAX_LIB_SLOTS; s++){
			if (lib_toc[s].status == 0xFF){
				seen_empty = 1;
			} else if (seen_empty && lib_toc[s].status == 0x01){
				needs_compact = 1;
				break;
			}
		}
		if (needs_compact){
			lib_rewrite_toc_compact();
		} else {
			lib_refresh_display_list();
		}
	}
}

uint8_t lib_save_program(const char *name, uint32_t size, uint8_t pruned){
	if (size == 0) return 0;
	int8_t slot = lib_find_free_slot();
	if (slot < 0){
		lib_rewrite_toc_compact();
		slot = lib_find_free_slot();
		if (slot < 0) return 0;
	}

	uint32_t offset = lib_calculate_next_pool_offset();
	if (offset + size > LIBRARY_POOL_SIZE){
		uint32_t total_active = lib_get_total_used_bytes();
		if (total_active + size > LIBRARY_POOL_SIZE) return 0; // Exceeds physical capacity

		// Defrag / Compact active programs using upper 128 kB of external parallel ROM as scratchpad
		CDC_Print("\r\n[Contiguous space needed: Defragmenting Library Partition...]\r\n");
		uint32_t scratch_addr = 0x20000;
		uint32_t cur_scratch = scratch_addr;
		uint32_t new_offsets[MAX_LIB_SLOTS];
		uint32_t cur_new_offset = 0;

		for (uint8_t s = 1; s < MAX_LIB_SLOTS; s++){
			if (lib_toc[s].status == 0x01){
				CDC_Printf("  Compacting Slot %02d: '%s' (%lu B)...\r\n", s, lib_toc[s].name, (unsigned long)lib_toc[s].size);
				new_offsets[s] = cur_new_offset;
				const uint8_t *pdata = (const uint8_t *)(LIBRARY_POOL_ADDR + lib_toc[s].flash_offset);
				initROMProgramming();
				for (uint32_t b = 0; b < lib_toc[s].size; b++){
					writeROMFast(cur_scratch + b, pdata[b]);
				}
				cur_scratch += lib_toc[s].size;
				cur_new_offset += (lib_toc[s].size + 3) & ~3;
			}
		}

		CDC_Print("Rewriting Flash pool... ");
		HAL_FLASH_Unlock();
		FLASH_EraseInitTypeDef erase;
		uint32_t err = 0;
		erase.TypeErase = FLASH_TYPEERASE_PAGES;
		erase.PageAddress = LIBRARY_POOL_ADDR;
		erase.NbPages = LIBRARY_POOL_SIZE / 2048;
		HAL_FLASHEx_Erase(&erase, &err);

		cur_scratch = scratch_addr;
		for (uint8_t s = 1; s < MAX_LIB_SLOTS; s++){
			if (lib_toc[s].status == 0x01){
				initROMNormal();
				HAL_GPIO_WritePin(OE_GPIO_Port, OE_Pin, GPIO_PIN_RESET);
				uint32_t dst = LIBRARY_POOL_ADDR + new_offsets[s];
				for (uint32_t b = 0; b < lib_toc[s].size; b += 4){
					uint32_t w = 0;
					for (int k = 0; k < 4; k++){
						uint8_t byte_val = (b + k < lib_toc[s].size) ? readROM(cur_scratch + b + k) : 0xFF;
						w |= ((uint32_t)byte_val) << (k * 8);
					}
					HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, dst + b, w);
				}
				cur_scratch += lib_toc[s].size;
			}
		}
		HAL_FLASH_Lock();

		CDC_Print("Done.\r\n[Compaction complete: Active programs packed forward]\r\n\r\n");
		lib_rewrite_toc_compact();
		offset = cur_new_offset;
	}

	// Ensure destination pages in STM32 Flash are erased
	uint32_t start_page = (LIBRARY_POOL_ADDR + offset) & ~2047UL;
	uint32_t end_page = (LIBRARY_POOL_ADDR + offset + size + 2047UL) & ~2047UL;
	uint32_t nb_pages = (end_page - start_page) / 2048;

	HAL_FLASH_Unlock();
	for (uint32_t p = 0; p < nb_pages; p++){
		uint32_t paddr = start_page + (p * 2048);
		uint32_t *pw = (uint32_t *)paddr;
		uint8_t needs_erase = 0;
		for (int i = 0; i < 512; i++){
			if (pw[i] != 0xFFFFFFFF){ needs_erase = 1; break; }
		}
		if (needs_erase){
			FLASH_EraseInitTypeDef ep;
			uint32_t perr = 0;
			ep.TypeErase = FLASH_TYPEERASE_PAGES;
			ep.PageAddress = paddr;
			ep.NbPages = 1;
			HAL_FLASHEx_Erase(&ep, &perr);
		}
	}

	// Write program payload words (read from external parallel ROM address 0)
	initROMNormal();
	HAL_GPIO_WritePin(OE_GPIO_Port, OE_Pin, GPIO_PIN_RESET);
	uint32_t dst_addr = LIBRARY_POOL_ADDR + offset;
	for (uint32_t i = 0; i < size; i += 4){
		uint32_t w = 0;
		for (int k = 0; k < 4; k++){
			uint8_t byte_val = (i + k < size) ? readROM(i + k) : 0xFF;
			w |= ((uint32_t)byte_val) << (k * 8);
		}
		HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, dst_addr + i, w);
	}

	// Write TOC descriptor
	ProgramSlot slot_data;
	memset(&slot_data, 0xFF, sizeof(slot_data));
	slot_data.desc.status = 0x01;
	strncpy(slot_data.desc.name, name, LIB_NAME_MAX_LEN - 1);
	slot_data.desc.name[LIB_NAME_MAX_LEN - 1] = '\0';
	slot_data.desc.pruned = pruned;
	slot_data.desc.pad[0] = 0;
	slot_data.desc.pad[1] = 0;
	slot_data.desc.flash_offset = offset;
	slot_data.desc.size = size;
	slot_data.desc.crc32 = 0;

	uint32_t desc_addr = LIBRARY_TOC_ADDR + (slot * sizeof(ProgramDescriptor));
	__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_WRPERR | FLASH_FLAG_PGERR);
	for (uint32_t w = 0; w < sizeof(ProgramDescriptor)/4; w++){
		HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, desc_addr + (w * 4), slot_data.words[w]);
	}

	HAL_FLASH_Lock();
	lib_refresh_display_list();
	return slot;
}

void lib_delete_program(uint8_t slot){
	if (slot == 0 || slot >= MAX_LIB_SLOTS) return;
	if (lib_toc[slot].status == 0x01){
		HAL_FLASH_Unlock();
		__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_WRPERR | FLASH_FLAG_PGERR);
		HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, LIBRARY_TOC_ADDR + (slot * sizeof(ProgramDescriptor)), 0x0000);
		HAL_FLASH_Lock();
		lib_rewrite_toc_compact();
	}
}

void lib_load_and_run(uint8_t slot){
	if (slot == 0){
		flashDefaultLogoProgram();
		menu_exit();
		return;
	}
	if (slot >= MAX_LIB_SLOTS || lib_toc[slot].status != 0x01) return;

	uint32_t size = lib_toc[slot].size;
	const uint8_t *src = (const uint8_t *)(LIBRARY_POOL_ADDR + lib_toc[slot].flash_offset);

	flashBufferToROM(src, size, (const char *)lib_toc[slot].name, cfg_append_endless_loop, cfg_auto_reset_after_pgm);
	menu_exit();
}

void lib_dump_program(uint8_t slot){
	if (slot >= MAX_LIB_SLOTS) return;
	const char *pname;
	const uint8_t *src;
	uint32_t psize;

	if (slot == 0){
		pname = "Brainfuino Demo";
		src = (const uint8_t *)DEFAULT_BRAINFUINO_LOGO_BF;
		psize = sizeof(DEFAULT_BRAINFUINO_LOGO_BF) - 1;
	} else {
		if (lib_toc[slot].status != 0x01) return;
		pname = (const char *)lib_toc[slot].name;
		src = (const uint8_t *)(LIBRARY_POOL_ADDR + lib_toc[slot].flash_offset);
		psize = lib_toc[slot].size;
	}

	CDC_Printf("!DUMP:START:%u:%s:%lu\r\n", (unsigned int)slot, pname, (unsigned long)psize);
	CDC_Write(src, psize);
	CDC_Print("\r\n!DUMP:END\r\n");
	if (state == STATE_CONFIG){
		CDC_Print("Press any key to return to library menu...\r\n");
		lib_dump_viewing = 1;
	}
}

void lib_start_add_program(void){
	lib_code_size = 0;
	lib_name_len = 0;
	memset(lib_name_buf, 0, sizeof(lib_name_buf));
	lib_add_substate = LIB_ADD_NAME;
	state = STATE_LIB_ADD;
	CDC_Print("\x1b[2J\x1b[H\r\n");
	CDC_Print("+-------------------------------------------------+\r\n");
	CDC_Print("|            ADD PROGRAM TO LIBRARY               |\r\n");
	CDC_Print("+-------------------------------------------------+\r\n");
	CDC_Print("Enter program name (max 15 chars, or ESC to cancel):\r\n> ");
}

static uint8_t get_menu_item_count(void){
	if (menu_page == MENU_PAGE_MAIN){
		return 6;
	} else if (menu_page == MENU_PAGE_SETTINGS){
		return cfg_manual_step_enabled ? 13 : 11;
	} else if (menu_page == MENU_PAGE_LIBRARY){
		lib_refresh_display_list();
		return lib_display_count + 2;
	}
	return 6;
}

static const char *get_step_ticks_name(uint8_t t){
	switch(t){
		case STEP_TICKS_1:    return "1 Tick";
		case STEP_TICKS_10:   return "10 Ticks";
		case STEP_TICKS_100:  return "100 Ticks";
		case STEP_TICKS_1K:   return "1k Ticks";
		case STEP_TICKS_10K:  return "10k Ticks";
		case STEP_TICKS_100K: return "100k Ticks";
		default:              return "100 Ticks";
	}
}

static const char *get_step_key_name(uint8_t k){
	switch(k){
		case STEP_KEY_SPACE: return "Spacebar";
		case STEP_KEY_TAB:   return "Tab";
		case STEP_KEY_ENTER: return "Enter";
		default:             return "Spacebar";
	}
}

void menu_render_main(void){
	CDC_Print("+-------------------------------------------------+\r\n");
	CDC_Print("|          BRAINFUINO CONFIGURATION MENU          |\r\n");
	CDC_Print("+-------------------------------------------------+\r\n");
	CDC_Print("|  Use [Up/Down] & [Enter], or type [1-5, 0]      |\r\n");
	CDC_Print("+-------------------------------------------------+\r\n");

	const char *labels[6] = {
		"1. Program Library       -->",
		"2. Hardware Settings     -->",
		"3. Reboot to USB DFU        ",
		"4. Restore Factory Demo     ",
		"5. Save & Exit              ",
		"0. Exit without Saving      "
	};
	const char *vals[6] = {
		"[  OPEN  ]",
		"[  OPEN  ]",
		"[ REBOOT ]",
		"[ RESTORE]",
		"[  EXIT  ]",
		"[ CANCEL ]"
	};

	if (menu_cursor >= 6) menu_cursor = 5;

	for (int i = 0; i < 6; i++){
		if (i == menu_cursor){
			CDC_Printf("| \x1b[7m> %-28s %-10s <\x1b[0m |\r\n", labels[i], vals[i]);
		} else {
			CDC_Printf("|   %-28s %-10s   |\r\n", labels[i], vals[i]);
		}
	}

	CDC_Print("+-------------------------------------------------+\r\n");
	CDC_Print("|  Hardware: STM32F072 | Parallel ROM: 256 kB     |\r\n");
	CDC_Print("+-------------------------------------------------+\r\n");
	CDC_Print("Select option or use arrows + Enter: ");
}

void menu_render_settings(void){
	CDC_Print("+-------------------------------------------------+\r\n");
	CDC_Print("|          BRAINFUINO HARDWARE SETTINGS           |\r\n");
	CDC_Print("+-------------------------------------------------+\r\n");
	CDC_Print("|  Use [Up/Down], [Left/Right], or [Enter]        |\r\n");
	CDC_Print("+-------------------------------------------------+\r\n");

	uint8_t count = cfg_manual_step_enabled ? 13 : 11;
	if (menu_cursor >= count) menu_cursor = count - 1;

	for (int i = 0; i < count; i++){
		char val_str[16];
		const char *label = "";
		switch(i){
			case 0:
				label = "1. Auto-Program on Paste  ";
				snprintf(val_str, sizeof(val_str), "[ %-8s ]", cfg_auto_prog_run_mode ? "ENABLED" : "DISABLED");
				break;
			case 1:
				label = "2. Paste Upload Threshold ";
				snprintf(val_str, sizeof(val_str), "[   %2lu B   ]", (unsigned long)cfg_auto_prog_min_bytes);
				break;
			case 2:
				label = "3. Auto-Reset after Flash ";
				snprintf(val_str, sizeof(val_str), "[ %-8s ]", cfg_auto_reset_after_pgm ? "ENABLED" : "DISABLED");
				break;
			case 3:
				label = "4. Append Endless Loop    ";
				snprintf(val_str, sizeof(val_str), "[  %-6s  ]", cfg_append_endless_loop ? "[-]+[]" : "NONE");
				break;
			case 4:
				label = "5. FPGA Clock Frequency   ";
				snprintf(val_str, sizeof(val_str), "[ %-8s ]", get_freq_name(freq));
				break;
			case 5:
				label = "6. Prune non-BF on Paste  ";
				snprintf(val_str, sizeof(val_str), "[ %-8s ]", cfg_prune_on_paste ? "ENABLED" : "DISABLED");
				break;
			case 6:
				label = "7. Prune non-BF in Library";
				snprintf(val_str, sizeof(val_str), "[ %-8s ]", cfg_prune_in_library ? "ENABLED" : "DISABLED");
				break;
			case 7:
				label = "8. Serial Input CR -> LF  ";
				snprintf(val_str, sizeof(val_str), "[ %-8s ]", cfg_cr_to_lf ? "ENABLED" : "DISABLED");
				break;
			case 8:
				label = "9. Run Mode Speed Hotkeys ";
				{
					const char *hname = "NONE";
					if (cfg_speed_hotkey_mode == SPEED_HOTKEY_PGUP_PGDN) hname = "PgUp/Dn";
					else if (cfg_speed_hotkey_mode == SPEED_HOTKEY_UP_DOWN) hname = "Up/Down";
					else if (cfg_speed_hotkey_mode == SPEED_HOTKEY_PLUS_MINUS) hname = "+ / -";
					snprintf(val_str, sizeof(val_str), "[ %-8s ]", hname);
				}
				break;
			case 9:
				label = "A. Manual Stepping Mode   ";
				snprintf(val_str, sizeof(val_str), "[ %-8s ]", cfg_manual_step_enabled ? "ENABLED" : "DISABLED");
				break;
			case 10:
				if (cfg_manual_step_enabled){
					label = "T. Step Advance Ticks     ";
					snprintf(val_str, sizeof(val_str), "[ %-8s ]", get_step_ticks_name(cfg_manual_step_ticks));
				} else {
					label = "0. Back to Main Menu      ";
					snprintf(val_str, sizeof(val_str), "[   BACK   ]");
				}
				break;
			case 11:
				label = "C. Step Trigger Key       ";
				snprintf(val_str, sizeof(val_str), "[ %-8s ]", get_step_key_name(cfg_manual_step_key));
				break;
			case 12:
				label = "0. Back to Main Menu      ";
				snprintf(val_str, sizeof(val_str), "[   BACK   ]");
				break;
		}

		if (i == menu_cursor){
			CDC_Printf("| \x1b[7m> %s : %-12s <\x1b[0m |\r\n", label, val_str);
		} else {
			CDC_Printf("|   %s : %-12s   |\r\n", label, val_str);
		}
	}

	CDC_Print("+-------------------------------------------------+\r\n");
	CDC_Print("|  Hardware: STM32F072 | Parallel ROM: 256 kB     |\r\n");
	CDC_Print("+-------------------------------------------------+\r\n");
	CDC_Print("Select option or use arrows + Enter: ");
}

void menu_render_library(void){
	lib_refresh_display_list();
	uint8_t count = lib_display_count + 2;
	if (menu_cursor >= count) menu_cursor = count - 1;

	CDC_Print("+-------------------------------------------------+\r\n");
	CDC_Print("|            BRAINFUINO PROGRAM LIBRARY           |\r\n");
	CDC_Print("+-------------------------------------------------+\r\n");
	CDC_Print("| [Enter/E] Run [A] Add [D/Del] Del [B/0] Back    |\r\n");
	CDC_Print("+-------------------------------------------------+\r\n");

	for (int i = 0; i < count; i++){
		char line_buf[80];
		if (i < lib_display_count){
			uint8_t slot = lib_display_slots[i];
			const char *pname = (slot == 0) ? "Brainfuino Demo" : (const char *)lib_toc[slot].name;
			uint32_t psize = (slot == 0) ? (sizeof(DEFAULT_BRAINFUINO_LOGO_BF) - 1) : lib_toc[slot].size;
			char size_str[16];
			if (psize < 1024){
				snprintf(size_str, sizeof(size_str), "%3lu B", (unsigned long)psize);
			} else {
				snprintf(size_str, sizeof(size_str), "%2lu.%1lukB", (unsigned long)(psize / 1024), (unsigned long)((psize % 1024) * 10 / 1024));
			}

			if (i == menu_cursor){
				snprintf(line_buf, sizeof(line_buf), "| \x1b[7m> %02d. %-28s  [%7s] <\x1b[0m |", i, pname, size_str);
			} else {
				snprintf(line_buf, sizeof(line_buf), "|   %02d. %-28s  [%7s]   |", i, pname, size_str);
			}
		}
		else if (i == lib_display_count){
			if (i == menu_cursor){
				snprintf(line_buf, sizeof(line_buf), "| \x1b[7m> [ + Add New Program ]                       <\x1b[0m |");
			} else {
				snprintf(line_buf, sizeof(line_buf), "|   [ + Add New Program ]                         |");
			}
		}
		else {
			if (i == menu_cursor){
				snprintf(line_buf, sizeof(line_buf), "| \x1b[7m> 0. Back to Main Menu           [   BACK   ] <\x1b[0m |");
			} else {
				snprintf(line_buf, sizeof(line_buf), "|   0. Back to Main Menu             [   BACK   ] |");
			}
		}
		CDC_Printf("%s\r\n", line_buf);
	}

	CDC_Print("+-------------------------------------------------+\r\n");
	uint32_t used_bytes = lib_get_total_used_bytes();
	uint32_t used_kb = used_bytes / 1024;
	uint32_t used_frac = (used_bytes % 1024) * 10 / 1024;
	CDC_Printf("|  Library: %2d / 63 Programs | Used: %2lu.%1lu / 76 kB |\r\n",
	           lib_display_count - 1, (unsigned long)used_kb, (unsigned long)used_frac);
	CDC_Print("+-------------------------------------------------+\r\n");
	CDC_Print("Select program [Enter/E=Run, D/Del=Delete, B=Back, A=Add]: ");
}

void menu_render(void){
	CDC_Print("\x1b[2J\x1b[H\r\n");
	if (menu_page == MENU_PAGE_MAIN){
		menu_render_main();
	} else if (menu_page == MENU_PAGE_SETTINGS){
		menu_render_settings();
	} else if (menu_page == MENU_PAGE_LIBRARY){
		menu_render_library();
	}
}

void menu_execute_action(uint8_t item, int8_t dir){
	if (menu_page == MENU_PAGE_MAIN){
		switch(item){
			case 0:
				menu_page = MENU_PAGE_LIBRARY;
				menu_cursor = 0;
				menu_needs_render = 1;
				break;
			case 1:
				menu_page = MENU_PAGE_SETTINGS;
				menu_cursor = 0;
				menu_needs_render = 1;
				break;
			case 2:
				Execute_DFU_Jump();
				break;
			case 3:
				CDC_Print("\r\nRestoring Default Demo to Flash...\r\n");
				flashDefaultLogoProgram();
				break;
			case 4:
				settings_save();
				menu_exit();
				break;
			case 5:
				settings_load();
				menu_exit();
				break;
			default:
				break;
		}
	}
	else if (menu_page == MENU_PAGE_SETTINGS){
		switch(item){
			case 0:
				cfg_auto_prog_run_mode = !cfg_auto_prog_run_mode;
				break;
			case 1:
				if (dir > 0){
					if (cfg_auto_prog_min_bytes == 4) cfg_auto_prog_min_bytes = 8;
					else if (cfg_auto_prog_min_bytes == 8) cfg_auto_prog_min_bytes = 16;
					else if (cfg_auto_prog_min_bytes == 16) cfg_auto_prog_min_bytes = 32;
					else if (cfg_auto_prog_min_bytes == 32) cfg_auto_prog_min_bytes = 64;
					else cfg_auto_prog_min_bytes = 4;
				} else {
					if (cfg_auto_prog_min_bytes == 64) cfg_auto_prog_min_bytes = 32;
					else if (cfg_auto_prog_min_bytes == 32) cfg_auto_prog_min_bytes = 16;
					else if (cfg_auto_prog_min_bytes == 16) cfg_auto_prog_min_bytes = 8;
					else if (cfg_auto_prog_min_bytes == 8) cfg_auto_prog_min_bytes = 4;
					else cfg_auto_prog_min_bytes = 64;
				}
				break;
			case 2:
				cfg_auto_reset_after_pgm = !cfg_auto_reset_after_pgm;
				break;
			case 3:
				cfg_append_endless_loop = !cfg_append_endless_loop;
				break;
			case 4:
				if (dir > 0){
					set_freq((freq + 1) % FREQ_COUNT);
				} else {
					set_freq((freq + FREQ_COUNT - 1) % FREQ_COUNT);
				}
				break;
			case 5:
				cfg_prune_on_paste = !cfg_prune_on_paste;
				break;
			case 6:
				cfg_prune_in_library = !cfg_prune_in_library;
				break;
			case 7:
				cfg_cr_to_lf = !cfg_cr_to_lf;
				break;
			case 8:
				if (dir > 0){
					cfg_speed_hotkey_mode = (cfg_speed_hotkey_mode + 1) % 4;
				} else {
					cfg_speed_hotkey_mode = (cfg_speed_hotkey_mode + 3) % 4;
				}
				break;
			case 9:
				cfg_manual_step_enabled = !cfg_manual_step_enabled;
				break;
			case 10:
				if (cfg_manual_step_enabled){
					if (dir > 0){
						cfg_manual_step_ticks = (cfg_manual_step_ticks + 1) % 6;
					} else {
						cfg_manual_step_ticks = (cfg_manual_step_ticks + 5) % 6;
					}
				} else {
					menu_page = MENU_PAGE_MAIN;
					menu_cursor = 1;
					menu_needs_render = 1;
				}
				break;
			case 11:
				if (dir > 0){
					cfg_manual_step_key = (cfg_manual_step_key + 1) % 3;
				} else {
					cfg_manual_step_key = (cfg_manual_step_key + 2) % 3;
				}
				break;
			case 12:
				menu_page = MENU_PAGE_MAIN;
				menu_cursor = 1;
				menu_needs_render = 1;
				break;
			default:
				break;
		}
	}
	else if (menu_page == MENU_PAGE_LIBRARY){
		lib_refresh_display_list();
		if (item < lib_display_count){
			uint8_t slot = lib_display_slots[item];
			lib_load_and_run(slot);
		}
		else if (item == lib_display_count){
			lib_start_add_program();
		}
		else {
			menu_page = MENU_PAGE_MAIN;
			menu_cursor = 0;
			menu_needs_render = 1;
		}
	}
}

#define SETTINGS_FLASH_ADDR   0x0801F800UL
#define SETTINGS_MAGIC        0xBF072C04UL

typedef struct {
	uint32_t magic;
	uint8_t  freq;
	uint8_t  auto_prog_run_mode;
	uint8_t  auto_reset_after_pgm;
	uint8_t  append_endless_loop;
	uint8_t  speed_hotkey_mode;
	uint8_t  manual_step_enabled;
	uint8_t  manual_step_ticks;
	uint8_t  manual_step_key;
	uint8_t  prune_on_paste;
	uint8_t  prune_in_library;
	uint8_t  cr_to_lf;
	uint8_t  reserved;
	uint32_t auto_prog_min_bytes;
	uint32_t checksum;
} PersistentSettings;

static uint32_t calc_settings_checksum(const PersistentSettings *s){
	return s->magic + (uint32_t)s->freq + (uint32_t)s->auto_prog_run_mode +
	       (uint32_t)s->auto_reset_after_pgm + (uint32_t)s->append_endless_loop +
	       (uint32_t)s->speed_hotkey_mode + (uint32_t)s->manual_step_enabled +
	       (uint32_t)s->manual_step_ticks + (uint32_t)s->manual_step_key +
	       (uint32_t)s->prune_on_paste + (uint32_t)s->prune_in_library +
	       (uint32_t)s->cr_to_lf + s->auto_prog_min_bytes;
}

void settings_load(void){
	const PersistentSettings *flash_cfg = (const PersistentSettings *)SETTINGS_FLASH_ADDR;
	if (flash_cfg->magic == SETTINGS_MAGIC && flash_cfg->checksum == calc_settings_checksum(flash_cfg)){
		if (flash_cfg->freq < FREQ_COUNT) freq = flash_cfg->freq;
		else freq = DEFAULT_FREQ_INDEX;

		cfg_auto_prog_run_mode = flash_cfg->auto_prog_run_mode ? 1 : 0;
		cfg_auto_reset_after_pgm = flash_cfg->auto_reset_after_pgm ? 1 : 0;
		cfg_append_endless_loop = flash_cfg->append_endless_loop ? 1 : 0;
		cfg_speed_hotkey_mode = (flash_cfg->speed_hotkey_mode <= 3) ? flash_cfg->speed_hotkey_mode : SPEED_HOTKEY_PGUP_PGDN;
		cfg_manual_step_enabled = flash_cfg->manual_step_enabled ? 1 : 0;
		cfg_manual_step_ticks = (flash_cfg->manual_step_ticks <= 5) ? flash_cfg->manual_step_ticks : STEP_TICKS_100;
		cfg_manual_step_key = (flash_cfg->manual_step_key <= 2) ? flash_cfg->manual_step_key : STEP_KEY_SPACE;
		cfg_prune_on_paste = flash_cfg->prune_on_paste ? 1 : 0;
		cfg_prune_in_library = flash_cfg->prune_in_library ? 1 : 0;
		cfg_cr_to_lf = flash_cfg->cr_to_lf ? 1 : 0;

		if (flash_cfg->auto_prog_min_bytes >= 4 && flash_cfg->auto_prog_min_bytes <= 64){
			cfg_auto_prog_min_bytes = flash_cfg->auto_prog_min_bytes;
		} else {
			cfg_auto_prog_min_bytes = 16;
		}
	} else {
		freq = DEFAULT_FREQ_INDEX;
		cfg_auto_prog_run_mode = 1;
		cfg_auto_reset_after_pgm = 1;
		cfg_append_endless_loop = 1;
		cfg_speed_hotkey_mode = SPEED_HOTKEY_PGUP_PGDN;
		cfg_manual_step_enabled = 0;
		cfg_manual_step_ticks = STEP_TICKS_100;
		cfg_manual_step_key = STEP_KEY_SPACE;
		cfg_prune_on_paste = 0;
		cfg_prune_in_library = 1;
		cfg_cr_to_lf = 1;
		cfg_auto_prog_min_bytes = 16;
	}
}

void settings_save(void){
	PersistentSettings new_cfg;
	memset(&new_cfg, 0, sizeof(new_cfg));
	new_cfg.magic = SETTINGS_MAGIC;
	new_cfg.freq = freq;
	new_cfg.auto_prog_run_mode = cfg_auto_prog_run_mode;
	new_cfg.auto_reset_after_pgm = cfg_auto_reset_after_pgm;
	new_cfg.append_endless_loop = cfg_append_endless_loop;
	new_cfg.speed_hotkey_mode = cfg_speed_hotkey_mode;
	new_cfg.manual_step_enabled = cfg_manual_step_enabled;
	new_cfg.manual_step_ticks = cfg_manual_step_ticks;
	new_cfg.manual_step_key = cfg_manual_step_key;
	new_cfg.prune_on_paste = cfg_prune_on_paste;
	new_cfg.prune_in_library = cfg_prune_in_library;
	new_cfg.cr_to_lf = cfg_cr_to_lf;
	new_cfg.auto_prog_min_bytes = cfg_auto_prog_min_bytes;
	new_cfg.checksum = calc_settings_checksum(&new_cfg);

	const PersistentSettings *flash_cfg = (const PersistentSettings *)SETTINGS_FLASH_ADDR;
	if (memcmp(flash_cfg, &new_cfg, sizeof(new_cfg)) == 0){
		return; // Flash already up to date, save write cycles
	}

	HAL_FLASH_Unlock();

	FLASH_EraseInitTypeDef erase_init;
	uint32_t page_error = 0;
	erase_init.TypeErase = FLASH_TYPEERASE_PAGES;
	erase_init.PageAddress = SETTINGS_FLASH_ADDR;
	erase_init.NbPages = 1;

	if (HAL_FLASHEx_Erase(&erase_init, &page_error) == HAL_OK){
		uint32_t *src = (uint32_t *)&new_cfg;
		uint32_t words = sizeof(new_cfg) / sizeof(uint32_t);
		for (uint32_t i = 0; i < words; i++){
			HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, SETTINGS_FLASH_ADDR + (i * 4), src[i]);
		}
	}

	HAL_FLASH_Lock();
}

void menu_enter(void){
	state = STATE_CONFIG;
	menu_page = MENU_PAGE_MAIN;
	menu_cursor = 0;
	menu_action_pending = -1;
	menu_exit_requested = 0;
	menu_needs_render = 1;
	// Hold soft-processor in reset while configuring
	HAL_GPIO_WritePin(BF_RST_GPIO_Port, BF_RST_Pin, GPIO_PIN_RESET);
}

void menu_exit(void){
	settings_save();
	state = STATE_RUN;
	manual_step_ticks_pending = 0;
	HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
	LED_GPIO_Port->BRR = LED_Pin;
	CDC_Printf("\r\n\x1b[0m[Config Saved] Resuming Brainfuino (Clock: %s)...\r\n", get_freq_name(freq));
	if (cfg_manual_step_enabled){
		CDC_Printf("[Manual Step Mode Active: Press %s to step %s]\r\n",
		           get_step_key_name(cfg_manual_step_key), get_step_ticks_name(cfg_manual_step_ticks));
	}
	initROMNormal();
	wait(1000);
	HAL_GPIO_WritePin(OE_GPIO_Port, OE_Pin, GPIO_PIN_RESET);
	wait(1000);
	set_freq(freq);
	// Pulse FPGA reset to restart execution cleanly with any new settings
	HAL_GPIO_WritePin(BF_RST_GPIO_Port, BF_RST_Pin, GPIO_PIN_RESET);
	HAL_Delay(10);
	HAL_GPIO_WritePin(BF_RST_GPIO_Port, BF_RST_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
	LED_GPIO_Port->BRR = LED_Pin;
}

void menu_export_dynamic_schema(void){
	CDC_Print("!MENU:START:13\r\n");
	// Folders (Category groupings)
	CDC_Print("0;FOLDER;0;Execution & Clock\r\n");
	CDC_Print("1;FOLDER;0;Auto-Programming\r\n");
	CDC_Print("2;FOLDER;0;Input & Step Control\r\n");
	CDC_Print("3;FOLDER;0;System Commands\r\n");

	// Folder 0: Execution & Clock
	// Field 10: Clock Frequency (SELECT)
	CDC_Printf("10;SELECT;0;FPGA Clock Frequency;%u;", (unsigned int)freq);
	for (uint8_t i = 0; i < FREQ_COUNT; i++){
		CDC_Printf("%s%s", get_freq_name(i), (i + 1 < FREQ_COUNT) ? ";" : "\r\n");
	}

	// Field 11: Speed Hotkey Mode (SELECT)
	CDC_Printf("11;SELECT;0;Run Mode Hotkeys;%u;NONE;PgUp/PgDn;Up/Down;+ / -\r\n", (unsigned int)cfg_speed_hotkey_mode);

	// Folder 1: Auto-Programming
	// Field 12: Auto-Program on Paste (TOGGLE)
	CDC_Printf("12;TOGGLE;1;Auto-Program on Paste;%u;DISABLED;ENABLED\r\n", (unsigned int)cfg_auto_prog_run_mode);

	// Field 13: Paste Upload Threshold (SELECT)
	uint8_t thresh_idx = 0;
	if (cfg_auto_prog_min_bytes == 8) thresh_idx = 1;
	else if (cfg_auto_prog_min_bytes == 16) thresh_idx = 2;
	else if (cfg_auto_prog_min_bytes == 32) thresh_idx = 3;
	else if (cfg_auto_prog_min_bytes == 64) thresh_idx = 4;
	CDC_Printf("13;SELECT;1;Paste Threshold;%u;4 Bytes;8 Bytes;16 Bytes;32 Bytes;64 Bytes\r\n", (unsigned int)thresh_idx);

	// Field 14: Auto-Reset after Flash (TOGGLE)
	CDC_Printf("14;TOGGLE;1;Auto-Reset after Flash;%u;DISABLED;ENABLED\r\n", (unsigned int)cfg_auto_reset_after_pgm);

	// Field 15: Append Loop [-]+[] (TOGGLE)
	CDC_Printf("15;TOGGLE;1;Append Loop [-]+[];%u;DISABLED;ENABLED\r\n", (unsigned int)cfg_append_endless_loop);

	// Field 16: Prune non-BF on Paste (TOGGLE)
	CDC_Printf("16;TOGGLE;1;Prune non-BF on Paste;%u;DISABLED;ENABLED\r\n", (unsigned int)cfg_prune_on_paste);

	// Field 17: Prune non-BF in Library (TOGGLE)
	CDC_Printf("17;TOGGLE;1;Prune non-BF in Library;%u;DISABLED;ENABLED\r\n", (unsigned int)cfg_prune_in_library);

	// Folder 2: Input & Step Control
	// Field 18: Serial CR to LF (TOGGLE)
	CDC_Printf("18;TOGGLE;2;Serial Input CR to LF;%u;DISABLED;ENABLED\r\n", (unsigned int)cfg_cr_to_lf);

	// Field 19: Manual Stepping Mode (TOGGLE)
	CDC_Printf("19;TOGGLE;2;Manual Stepping Mode;%u;DISABLED;ENABLED\r\n", (unsigned int)cfg_manual_step_enabled);

	// Field 20: Step Advance Ticks (SELECT)
	CDC_Printf("20;SELECT;2;Step Advance Ticks;%u;1 Tick;10 Ticks;100 Ticks;1,000 Ticks;10,000 Ticks;100,000 Ticks\r\n",
	           (unsigned int)(cfg_manual_step_ticks <= 5 ? cfg_manual_step_ticks : 2));

	// Field 21: Step Trigger Key (SELECT)
	CDC_Printf("21;SELECT;2;Step Trigger Key;%u;Spacebar;Tab;Enter\r\n",
	           (unsigned int)(cfg_manual_step_key <= 2 ? cfg_manual_step_key : 0));

	// Folder 3: System Commands
	CDC_Print("30;COMMAND;3;Reboot to USB DFU\r\n");
	CDC_Print("31;COMMAND;3;Restore Factory Demo\r\n");
	CDC_Print("32;COMMAND;3;Soft Reset FPGA\r\n");

	CDC_Print("!MENU:END\r\n");
}

void lib_export_dynamic_schema(void){
	uint8_t count = 0;
	if (lib_toc[0].crc32 == TOC_MAGIC) count++;
	for (uint8_t s = 1; s < MAX_LIB_SLOTS; s++){
		if (lib_toc[s].status == 0x01) count++;
	}
	uint32_t used = lib_get_total_used_bytes();
	CDC_Printf("!LIB:START:%u:%lu:%lu\r\n", (unsigned int)count, (unsigned long)used, (unsigned long)LIBRARY_POOL_SIZE);

	// Slot 0: Brainfuino Factory Demo (Protected)
	uint32_t demo_size = sizeof(DEFAULT_BRAINFUINO_LOGO_BF) - 1;
	CDC_Printf("!LIB:PROG:0;Brainfuino;%lu;1;1\r\n", (unsigned long)demo_size);

	// Slots 1..63: User-installed programs
	for (uint8_t s = 1; s < MAX_LIB_SLOTS; s++){
		if (lib_toc[s].status == 0x01){
			CDC_Printf("!LIB:PROG:%u;%s;%lu;%u;0\r\n",
			           (unsigned int)s,
			           lib_toc[s].name,
			           (unsigned long)lib_toc[s].size,
			           (unsigned int)lib_toc[s].pruned);
		}
	}
	CDC_Print("!LIB:END\r\n");
}

static uint8_t is_prefix_ci(const uint8_t *b, uint32_t len, const char *prefix){
	uint32_t plen = strlen(prefix);
	if (len < plen) return 0;
	for (uint32_t i = 0; i < plen; i++){
		char c1 = (char)b[i];
		char c2 = prefix[i];
		if (c1 >= 'a' && c1 <= 'z') c1 -= 32;
		if (c2 >= 'a' && c2 <= 'z') c2 -= 32;
		if (c1 != c2) return 0;
	}
	return 1;
}

static uint8_t is_cmd_ci(const uint8_t *b, uint32_t len, const char *cmd){
	uint32_t clen = strlen(cmd);
	if (len < clen) return 0;
	for (uint32_t i = 0; i < clen; i++){
		char c1 = (char)b[i];
		char c2 = cmd[i];
		if (c1 >= 'a' && c1 <= 'z') c1 -= 32;
		if (c2 >= 'a' && c2 <= 'z') c2 -= 32;
		if (c1 != c2) return 0;
	}
	for (uint32_t i = clen; i < len; i++){
		if (b[i] != '\r' && b[i] != '\n' && b[i] != ' ' && b[i] != '\t') return 0;
	}
	return 1;
}

uint8_t CDC_Receive_Callback(uint8_t *buff, uint32_t len){
	// Automated USB DFU trigger command (from build script or terminal)
	if (len >= 5 && strncmp((char *)buff, "!DFU!", 5) == 0){
		dfu_requested = 1;
		return 1;
	}

	if (is_prefix_ci(buff, len, "!RESET") || is_prefix_ci(buff, len, "!RST")){
		reset_requested = 1;
		return 1;
	}

	// Program ROM Dump (!DUMP, !ROM:DUMP, or !)
	if (is_cmd_ci(buff, len, "!DUMP") || is_cmd_ci(buff, len, "!ROM:DUMP") || is_cmd_ci(buff, len, "!")){
		code_dump = 1;
		return 1;
	}

	// ExpressLRS-style Dynamic Parameter Schema Query (!MENU*)
	if (is_cmd_ci(buff, len, "!MENU*")){
		dynamic_schema_requested = 1;
		return 1;
	}

	// Dynamic Program Library Schema Query (!LIB* or !LIB:LIST)
	if (is_cmd_ci(buff, len, "!LIB*") || is_cmd_ci(buff, len, "!LIB:LIST")){
		dynamic_lib_requested = 1;
		return 1;
	}

	// Dynamic Library Execution (!LIB:RUN:<slot>)
	if (len >= 9 && is_prefix_ci(buff, len, "!LIB:RUN:")){
		uint32_t slot = 0;
		const char *p = (const char *)buff + 9;
		while (*p >= '0' && *p <= '9'){
			slot = slot * 10 + (*p - '0');
			p++;
		}
		cmd_lib_run_slot = (int8_t)slot;
		return 1;
	}

	// Dynamic Library Code Dump (!LIB:DUMP:<slot>)
	if (len >= 10 && is_prefix_ci(buff, len, "!LIB:DUMP:")){
		uint32_t slot = 0;
		const char *p = (const char *)buff + 10;
		while (*p >= '0' && *p <= '9'){
			slot = slot * 10 + (*p - '0');
			p++;
		}
		cmd_lib_dump_slot = (int8_t)slot;
		return 1;
	}

	// Dynamic Library Program Deletion (!LIB:DEL:<slot>)
	if (len >= 9 && is_prefix_ci(buff, len, "!LIB:DEL:")){
		uint32_t slot = 0;
		const char *p = (const char *)buff + 9;
		while (*p >= '0' && *p <= '9'){
			slot = slot * 10 + (*p - '0');
			p++;
		}
		cmd_lib_del_slot = (int8_t)slot;
		return 1;
	}

	// Dynamic Library Direct Add without Verification (!LIB:ADD_NOVERIFY:<name>)
	if (len >= 18 && is_prefix_ci(buff, len, "!LIB:ADD_NOVERIFY:")){
		const char *p = (const char *)buff + 18;
		uint32_t nlen = 0;
		while (*p && *p != '\r' && *p != '\n' && nlen < (LIB_NAME_MAX_LEN - 1)){
			if (*p >= 0x20 && *p <= 0x7E && *p != ';') {
				lib_name_buf[nlen++] = *p;
			}
			p++;
		}
		if (nlen == 0){
			strncpy(lib_name_buf, "User_Prog", sizeof(lib_name_buf));
			nlen = strlen(lib_name_buf);
		}
		lib_name_buf[nlen] = '\0';
		lib_name_len = (uint8_t)nlen;

		initROMProgramming();
		eraseROMFast();
		lib_code_size = 0;
		lib_raw_rx_size = 0;
		lib_add_last_rx_tick = HAL_GetTick();
		lib_auto_verify = 2; // 2 = direct commit without verification test
		state = STATE_LIB_ADD;
		lib_add_substate = LIB_ADD_PASTE;
		CDC_Printf("!LIB:ADD:READY:%s\r\n", lib_name_buf);
		return 1;
	}

	// Dynamic Library Direct Add (!LIB:ADD:<name>)
	if (len >= 9 && is_prefix_ci(buff, len, "!LIB:ADD:")){
		const char *p = (const char *)buff + 9;
		uint32_t nlen = 0;
		while (*p && *p != '\r' && *p != '\n' && nlen < (LIB_NAME_MAX_LEN - 1)){
			if (*p >= 0x20 && *p <= 0x7E && *p != ';') {
				lib_name_buf[nlen++] = *p;
			}
			p++;
		}
		if (nlen == 0){
			strncpy(lib_name_buf, "User_Prog", sizeof(lib_name_buf));
			nlen = strlen(lib_name_buf);
		}
		lib_name_buf[nlen] = '\0';
		lib_name_len = (uint8_t)nlen;

		initROMProgramming();
		eraseROMFast();
		lib_code_size = 0;
		lib_raw_rx_size = 0;
		lib_add_last_rx_tick = HAL_GetTick();
		lib_auto_verify = 1;
		state = STATE_LIB_ADD;
		lib_add_substate = LIB_ADD_PASTE;
		CDC_Printf("!LIB:ADD:READY:%s\r\n", lib_name_buf);
		return 1;
	}

	// Verification early commit / abort
	if (is_cmd_ci(buff, len, "!COMMIT") || is_cmd_ci(buff, len, "!LIB:COMMIT")){
		lib_verify_early_commit = 1;
		return 1;
	}
	if (is_cmd_ci(buff, len, "!CANCEL") || is_cmd_ci(buff, len, "!LIB:CANCEL")){
		reset_requested = 1;
		return 1;
	}

	// Standard VT100 Interactive Menu
	if (is_prefix_ci(buff, len, "!MENU") || is_prefix_ci(buff, len, "!CONFIG")){
		menu_enter();
		return 1;
	}

	// Dynamic Parameter Value Mutation (!SET:<id>=<val>)
	if (len >= 6 && is_prefix_ci(buff, len, "!SET:")){
		uint32_t id = 0, val = 0;
		const char *p = (const char *)buff + 5;
		while (*p >= '0' && *p <= '9'){
			id = id * 10 + (*p - '0');
			p++;
		}
		if (*p == '='){
			p++;
			while (*p >= '0' && *p <= '9'){
				val = val * 10 + (*p - '0');
				p++;
			}
			switch(id){
				case 10:
					if (val < FREQ_COUNT) set_freq((uint8_t)val);
					break;
				case 11:
					cfg_speed_hotkey_mode = (uint8_t)(val % 4);
					break;
				case 12:
					cfg_auto_prog_run_mode = val ? 1 : 0;
					break;
				case 13:
					{
						uint8_t th_table[] = { 4, 8, 16, 32, 64 };
						cfg_auto_prog_min_bytes = (val < 5) ? th_table[val] : 4;
					}
					break;
				case 14:
					cfg_auto_reset_after_pgm = val ? 1 : 0;
					break;
				case 15:
					cfg_append_endless_loop = val ? 1 : 0;
					break;
				case 16:
					cfg_prune_on_paste = val ? 1 : 0;
					break;
				case 17:
					cfg_prune_in_library = val ? 1 : 0;
					break;
				case 18:
					cfg_cr_to_lf = val ? 1 : 0;
					break;
				case 19:
					cfg_manual_step_enabled = val ? 1 : 0;
					set_freq(freq);
					break;
				case 20:
					if (val <= 5) cfg_manual_step_ticks = (uint8_t)val;
					break;
				case 21:
					if (val <= 2) cfg_manual_step_key = (uint8_t)val;
					break;
				default:
					break;
			}
			settings_dirty = 1;
			settings_dirty_tick = HAL_GetTick();
			CDC_Printf("!SET:OK:%lu=%lu\r\n", (unsigned long)id, (unsigned long)val);
			return 1;
		}
	}

	// Dynamic Parameter Command Execution (!CMD:<id>)
	if (len >= 6 && is_prefix_ci(buff, len, "!CMD:")){
		uint32_t cid = 0;
		const char *p = (const char *)buff + 5;
		while (*p >= '0' && *p <= '9'){
			cid = cid * 10 + (*p - '0');
			p++;
		}
		CDC_Printf("!CMD:OK:%lu\r\n", (unsigned long)cid);
		if (cid == 30){
			dfu_requested = 1;
		} else if (cid == 31){
			cmd_restore_requested = 1;
		} else if (cid == 32){
			reset_requested = 1;
		}
		return 1;
	}

	// FPGA Clock Frequency commands: !clk+, !clk-, !clk (silent adjustment, query prints)
	if (is_cmd_ci(buff, len, "!CLK+") || is_cmd_ci(buff, len, "!CLOCK+")){
		if (freq + 1 < FREQ_COUNT){
			set_freq(freq + 1);
			settings_dirty = 1;
			settings_dirty_tick = HAL_GetTick();
		}
		return 1;
	}
	if (is_cmd_ci(buff, len, "!CLK-") || is_cmd_ci(buff, len, "!CLOCK-")){
		if (freq > 0){
			set_freq(freq - 1);
			settings_dirty = 1;
			settings_dirty_tick = HAL_GetTick();
		}
		return 1;
	}
	if (is_cmd_ci(buff, len, "!CLK") || is_cmd_ci(buff, len, "!CLOCK")){
		CDC_Printf("[Clock: %s]\r\n", get_freq_name(freq));
		return 1;
	}

	if (state == STATE_RUN){

		if (cfg_auto_prog_run_mode && (len >= cfg_auto_prog_min_bytes)){
			uint8_t filtered[64];
			uint32_t flen = 0;
			if (cfg_prune_on_paste){
				for (uint32_t i = 0; i < len && flen < sizeof(filtered); i++){
					if (is_bf_cmd((char)buff[i])) filtered[flen++] = buff[i];
				}
				if (flen == 0) return 1;
			} else {
				flen = (len < sizeof(filtered)) ? len : sizeof(filtered);
				memcpy(filtered, buff, flen);
			}

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

			memcpy(prog_buffer, filtered, flen);
			prog_rx_count = flen;
			prog_total_received = flen;

			CDC_Print("\r\n=== AUTO-PROGRAM MODE ===\r\nUploading Brainfuck code...\r\n");
			return 1;
		}

		// Run Mode dynamic speed switching hotkeys
		if (cfg_speed_hotkey_mode == SPEED_HOTKEY_PGUP_PGDN){
			if (len == 4 && buff[0] == 0x1B && buff[1] == '[' && buff[2] == '5' && buff[3] == '~'){
				speed_step_up();
				return 1;
			}
			if (len == 4 && buff[0] == 0x1B && buff[1] == '[' && buff[2] == '6' && buff[3] == '~'){
				speed_step_down();
				return 1;
			}
		}
		else if (cfg_speed_hotkey_mode == SPEED_HOTKEY_UP_DOWN){
			if (len == 3 && buff[0] == 0x1B && (buff[1] == '[' || buff[1] == 'O') && buff[2] == 'A'){
				speed_step_up();
				return 1;
			}
			if (len == 3 && buff[0] == 0x1B && (buff[1] == '[' || buff[1] == 'O') && buff[2] == 'B'){
				speed_step_down();
				return 1;
			}
		}
		else if (cfg_speed_hotkey_mode == SPEED_HOTKEY_PLUS_MINUS){
			if (len == 1 && buff[0] == '+'){
				speed_step_up();
				return 1;
			}
			if (len == 1 && buff[0] == '-'){
				speed_step_down();
				return 1;
			}
		}

		// Manual Step Mode clock pulse trigger
		if (cfg_manual_step_enabled){
			uint8_t match = 0;
			if (cfg_manual_step_key == STEP_KEY_SPACE && len == 1 && buff[0] == ' ') match = 1;
			else if (cfg_manual_step_key == STEP_KEY_TAB && len == 1 && buff[0] == '\t') match = 1;
			else if (cfg_manual_step_key == STEP_KEY_ENTER && (len == 1 && (buff[0] == '\r' || buff[0] == '\n'))) match = 1;

			if (match){
				uint32_t step_counts[] = { 1, 10, 100, 1000, 10000, 100000 };
				uint32_t burst = (cfg_manual_step_ticks <= 5) ? step_counts[cfg_manual_step_ticks] : 100;
				__disable_irq();
				manual_step_ticks_pending += burst;
				__enable_irq();
				return 1;
			}
		}

		// Pass characters cleanly to the running FPGA soft-processor via serial_inbox queue
		static uint8_t last_was_cr = 0;
		for (uint32_t i = 0; i < len; i++){
			uint8_t ch = buff[i];
			if (cfg_cr_to_lf){
				if (ch == '\r'){
					ch = '\n';
					last_was_cr = 1;
				} else if (ch == '\n' && last_was_cr){
					// Ignore LF immediately following CR (consume Windows CRLF pair as single LF)
					last_was_cr = 0;
					continue;
				} else {
					last_was_cr = 0;
				}
			}
			uint8_t next_tail = (serial_inbox_tail + 1) % SERIAL_INBOX_SIZE;
			if (next_tail != serial_inbox_head){
				serial_inbox[serial_inbox_tail] = ch;
				serial_inbox_tail = next_tail;
			}
		}

		// If FPGA is not currently busy reading an input byte, present the first byte immediately
		if (!serial_input && (serial_inbox_head != serial_inbox_tail)){
			uint8_t byte_to_send = serial_inbox[serial_inbox_head];
			serial_inbox_head = (serial_inbox_head + 1) % SERIAL_INBOX_SIZE;
			writeBFInput(byte_to_send);
			HAL_GPIO_WritePin(BF_INCMG_GPIO_Port, BF_INCMG_Pin, GPIO_PIN_SET);
			serial_input = 1;
		}
		return 1;
	}

	if (state == STATE_CONFIG){
		if (lib_dump_viewing){
			lib_dump_viewing = 0;
			menu_needs_render = 1;
			return 1;
		}
		static uint32_t last_num_key_tick = 0;
		if (len == 0) return 1;

		uint8_t count = get_menu_item_count();

		// Standalone Spacebar: execute/toggle selected item
		if (len == 1 && buff[0] == ' '){
			menu_action_pending = menu_cursor;
			menu_action_dir = 1;
			return 1;
		}

		uint32_t idx = 0;
		while (idx < len && (buff[idx] == ' ' || buff[idx] == '\t')) idx++;
		if (idx >= len) return 1;

		// 1. ANSI / VT100 arrow keys & Escape
		if (buff[idx] == 0x1B){
			if (idx + 2 < len && (buff[idx+1] == '[' || buff[idx+1] == 'O')){
				if (buff[idx+2] == 'A'){ // Up Arrow
					menu_cursor--;
					if (menu_cursor < 0) menu_cursor = count - 1;
					menu_needs_render = 1;
					return 1;
				}
				else if (buff[idx+2] == 'B'){ // Down Arrow
					menu_cursor++;
					if (menu_cursor >= count) menu_cursor = 0;
					menu_needs_render = 1;
					return 1;
				}
				else if (buff[idx+2] == 'C'){ // Right Arrow -> cycle forward in Settings only
					if (menu_page == MENU_PAGE_SETTINGS && menu_cursor < count - 1){
						menu_action_pending = menu_cursor;
						menu_action_dir = 1;
					}
					return 1;
				}
				else if (buff[idx+2] == 'D'){ // Left Arrow -> cycle backward in Settings only
					if (menu_page == MENU_PAGE_SETTINGS && menu_cursor < count - 1){
						menu_action_pending = menu_cursor;
						menu_action_dir = -1;
					}
					return 1;
				}
				else if (buff[idx+2] == '3' && idx + 3 < len && buff[idx+3] == '~'){ // VT100 Delete Key
					if (menu_page == MENU_PAGE_LIBRARY && menu_cursor < lib_display_count){
						uint8_t slot = lib_display_slots[menu_cursor];
						if (slot > 0){
							lib_delete_pending_slot = slot;
						}
					}
					return 1;
				}
			}
			// Standalone ESC: back to parent menu or exit
			if (menu_page == MENU_PAGE_SETTINGS || menu_page == MENU_PAGE_LIBRARY){
				menu_page = MENU_PAGE_MAIN;
				menu_cursor = 0;
				menu_needs_render = 1;
				return 1;
			} else {
				menu_exit_requested = 1;
				return 1;
			}
		}

		// 2. Direct numbered / letter shortcuts
		if (menu_page == MENU_PAGE_MAIN){
			if (buff[idx] >= '1' && buff[idx] <= '5'){
				last_num_key_tick = HAL_GetTick();
				menu_cursor = buff[idx] - '1';
				menu_action_pending = menu_cursor;
				menu_action_dir = 1;
				return 1;
			}
			if (buff[idx] == '0' || buff[idx] == 'q' || buff[idx] == 'Q' || buff[idx] == 'b' || buff[idx] == 'B' || buff[idx] == 0x08 || buff[idx] == 0x7F){
				menu_cursor = 5; // Exit without saving
				menu_action_pending = 5;
				menu_action_dir = 1;
				return 1;
			}
		}
		else if (menu_page == MENU_PAGE_SETTINGS){
			if (buff[idx] >= '1' && buff[idx] <= '9'){
				last_num_key_tick = HAL_GetTick();
				menu_cursor = buff[idx] - '1';
				menu_action_pending = menu_cursor;
				menu_action_dir = 1;
				return 1;
			}
			if (buff[idx] == 'a' || buff[idx] == 'A'){
				menu_cursor = 9;
				menu_action_pending = 9;
				menu_action_dir = 1;
				return 1;
			}
			if (cfg_manual_step_enabled && (buff[idx] == 't' || buff[idx] == 'T')){
				menu_cursor = 10;
				menu_action_pending = 10;
				menu_action_dir = 1;
				return 1;
			}
			if (cfg_manual_step_enabled && (buff[idx] == 'c' || buff[idx] == 'C' || buff[idx] == 'k' || buff[idx] == 'K')){
				menu_cursor = 11;
				menu_action_pending = 11;
				menu_action_dir = 1;
				return 1;
			}
			// 'b', 'B', '0', 'q', 'Q', Backspace (0x08, 0x7F) -> Back to main menu
			if (buff[idx] == '0' || buff[idx] == 'q' || buff[idx] == 'Q' || buff[idx] == 'b' || buff[idx] == 'B' || buff[idx] == 0x08 || buff[idx] == 0x7F){
				menu_page = MENU_PAGE_MAIN;
				menu_cursor = 1;
				menu_needs_render = 1;
				return 1;
			}
		}
		else if (menu_page == MENU_PAGE_LIBRARY){
			if (buff[idx] >= '1' && buff[idx] <= '9'){
				uint8_t sel_idx = buff[idx] - '0';
				if (sel_idx < lib_display_count){
					menu_cursor = sel_idx;
					menu_needs_render = 1;
				}
				return 1;
			}
			if (buff[idx] == 'a' || buff[idx] == 'A'){
				lib_start_add_requested = 1;
				return 1;
			}
			// 'd', 'D', 'x', 'X' -> Delete selected program
			if (buff[idx] == 'd' || buff[idx] == 'D' || buff[idx] == 'x' || buff[idx] == 'X'){
				if (menu_cursor < lib_display_count){
					uint8_t slot = lib_display_slots[menu_cursor];
					if (slot > 0){
						lib_delete_pending_slot = slot;
					}
				}
				return 1;
			}
			// 'e', 'E', 'l', 'L' -> Run selected program
			if (buff[idx] == 'e' || buff[idx] == 'E' || buff[idx] == 'l' || buff[idx] == 'L'){
				if (menu_cursor < lib_display_count){
					uint8_t slot = lib_display_slots[menu_cursor];
					lib_load_pending_slot = slot;
				}
				return 1;
			}
			// 'v', 'V' -> Dump/View selected program
			if (buff[idx] == 'v' || buff[idx] == 'V'){
				if (menu_cursor < lib_display_count){
					uint8_t slot = lib_display_slots[menu_cursor];
					lib_dump_pending_slot = slot;
				}
				return 1;
			}
			// 'b', 'B', '0', 'q', 'Q', Backspace (0x08, 0x7F) -> Back to main menu
			if (buff[idx] == 'b' || buff[idx] == 'B' || buff[idx] == '0' || buff[idx] == 'q' || buff[idx] == 'Q' || buff[idx] == 0x08 || buff[idx] == 0x7F){
				menu_page = MENU_PAGE_MAIN;
				menu_cursor = 0;
				menu_needs_render = 1;
				return 1;
			}
		}

		// 3. Enter / Return or Space: execute selected cursor item
		if (buff[idx] == '\r' || buff[idx] == '\n'){
			if (HAL_GetTick() - last_num_key_tick > 100){
				menu_action_pending = menu_cursor;
				menu_action_dir = 1;
			}
			return 1;
		}
		if (buff[idx] == ' '){
			menu_action_pending = menu_cursor;
			menu_action_dir = 1;
			return 1;
		}

		return 1;
	}

	if (state == STATE_LIB_ADD){
		if (lib_add_substate == LIB_ADD_NAME){
			char echo_buf[64];
			uint32_t echo_len = 0;
			for (uint32_t i = 0; i < len; i++){
				char c = (char)buff[i];
				if (c == 0x1B){ // ESC: cancel
					state = STATE_CONFIG;
					menu_page = MENU_PAGE_LIBRARY;
					menu_needs_render = 1;
					return 1;
				}
				if (c == '\r' || c == '\n'){
					if (lib_name_len == 0){
						strncpy(lib_name_buf, "User_Prog", sizeof(lib_name_buf));
						lib_name_len = strlen(lib_name_buf);
					}
					lib_name_buf[lib_name_len] = '\0';
					lib_add_substate = LIB_ADD_PASTE_PROMPT;
					return 1;
				}
				else if (c == 0x08 || c == 0x7F){ // Backspace
					if (lib_name_len > 0){
						lib_name_len--;
						lib_name_buf[lib_name_len] = '\0';
						if (echo_len + 3 < sizeof(echo_buf)){
							echo_buf[echo_len++] = '\b';
							echo_buf[echo_len++] = ' ';
							echo_buf[echo_len++] = '\b';
						}
					}
				}
				else if (c >= 0x20 && c <= 0x7E && lib_name_len < 15){
					lib_name_buf[lib_name_len++] = c;
					lib_name_buf[lib_name_len] = '\0';
					if (echo_len < sizeof(echo_buf)){
						echo_buf[echo_len++] = c;
					}
				}
			}
			if (echo_len > 0 && !TxBusy()){
				CDC_Transmit_FS((uint8_t *)echo_buf, echo_len);
			}
			return 1;
		}
		else if (lib_add_substate == LIB_ADD_PASTE){
			lib_add_last_rx_tick = HAL_GetTick();
			lib_raw_rx_size += len;
			for (uint32_t i = 0; i < len; i++){
				char c = (char)buff[i];
				if (cfg_prune_in_library && !is_bf_cmd(c)) continue;
				if (lib_code_size < FLASH_CAPACITY){
					writeROMFast(lib_code_size++, (uint8_t)c);
				}
			}
			return 1;
		}
		else if (lib_add_substate == LIB_ADD_CAPACITY_WARN){
			for (uint32_t i = 0; i < len; i++){
				char c = (char)buff[i];
				if (c == 0x1B || c == '0' || c == 'q' || c == 'Q'){
					state = STATE_CONFIG;
					menu_page = MENU_PAGE_LIBRARY;
					menu_needs_render = 1;
					return 1;
				}
				if (c >= '1' && c <= '9'){
					uint8_t slot = (uint8_t)(c - '0');
					if (i + 1 < len && buff[i+1] >= '0' && buff[i+1] <= '9'){
						slot = slot * 10 + (uint8_t)(buff[i+1] - '0');
						i++;
					}
					lib_capacity_delete_slot = slot;
					return 1;
				}
			}
			return 1;
		}
		else if (lib_add_substate == LIB_ADD_VERIFY_PROMPT){
			if (len > 0){
				char k = (char)buff[0];
				if (k == 'n' || k == 'N'){
					lib_verify_choice = 0;
					lib_verify_choice_pending = 1;
					return 1;
				}
				else if (k == 'y' || k == 'Y' || k == '\r' || k == '\n' || k == ' '){
					lib_verify_choice = 1;
					lib_verify_choice_pending = 1;
					return 1;
				}
			}
			return 1;
		}
		else if (lib_add_substate == LIB_ADD_VERIFYING){
			if (len > 0){
				if (buff[0] == '\r' || buff[0] == '\n' || buff[0] == ' '){
					lib_verify_early_commit = 1;
					return 1;
				}
				if (buff[0] == 0x1B || buff[0] == 'c' || buff[0] == 'C'){
					reset_requested = 1;
					return 1;
				}
			}
			return 1;
		}
	}

	if (state == STATE_PROGRAM){
		prog_last_rx_tick = HAL_GetTick();
		prog_active = 1;
		prog_raw_received += len;

		uint8_t filtered_buff[64];
		const uint8_t *in_data = buff;
		uint32_t in_len = len;

		if (cfg_prune_on_paste){
			uint32_t f_len = 0;
			for (uint32_t i = 0; i < len && f_len < sizeof(filtered_buff); i++){
				if (is_bf_cmd((char)buff[i])){
					filtered_buff[f_len++] = buff[i];
				}
			}
			if (f_len == 0) return 1;
			in_data = filtered_buff;
			in_len = f_len;
		}

		if (!prog_is_streaming){
			if (prog_rx_count + in_len <= PROG_BUF_SIZE){
				memcpy(prog_buffer + prog_rx_count, in_data, in_len);
				prog_rx_count += in_len;
				prog_total_received += in_len;
				return 1; // Buffer has room: re-arm USB endpoint immediately
			}
			else {
				// Buffer has filled 4 kB: fill remainder of prog_buffer and transition to streaming
				uint32_t space = PROG_BUF_SIZE - prog_rx_count;
				if (space > 0){
					memcpy(prog_buffer + prog_rx_count, in_data, space);
					prog_rx_count += space;
					prog_total_received += space;
				}
				prog_is_streaming = 1;
				uint32_t rem = in_len - space;
				for (uint32_t i = 0; i < rem; i++){
					uint16_t next = (staging_head + 1) % STAGING_BUF_SIZE;
					if (next != staging_tail){
						stream_staging[staging_head] = in_data[space + i];
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
			for (uint32_t i = 0; i < in_len; i++){
				if (prog_total_received < FLASH_CAPACITY){
					uint16_t next = (staging_head + 1) % STAGING_BUF_SIZE;
					if (next != staging_tail){
						stream_staging[staging_head] = in_data[i];
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

  __HAL_RCC_SYSCFG_CLK_ENABLE();
  __HAL_SYSCFG_REMAPMEMORY_FLASH();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();

  // Force clean USB re-enumeration so PC host disconnects from any previous DFU session
  __HAL_RCC_GPIOA_CLK_ENABLE();
  GPIO_InitTypeDef usb_disc = {0};
  usb_disc.Pin = GPIO_PIN_12;
  usb_disc.Mode = GPIO_MODE_OUTPUT_PP;
  usb_disc.Pull = GPIO_NOPULL;
  usb_disc.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &usb_disc);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_RESET);
  HAL_Delay(100);
  HAL_GPIO_DeInit(GPIOA, GPIO_PIN_12); // Release PA12 back to analog/USB mode

  MX_USB_DEVICE_Init();
  MX_ADC_Init();
  MX_TIM1_Init();
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

  // Load persistent settings and library TOC from internal Flash
  settings_load();
  lib_init_toc();
  set_freq(freq);

  uint8_t init_btn = HAL_GPIO_ReadPin(BRD_RST_GPIO_Port, BRD_RST_Pin);
  btn_last_sample = init_btn;
  btn_is_pressed = (init_btn == GPIO_PIN_RESET) ? 1 : 0;
  btn_last_stable_tick = HAL_GetTick();
  btn_press_tick = 0;
  btn_held_1s = 0;
  btn_held_3s = 0;
  btn_held_8s = 0;

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
	  // Check for dynamic schema query request (!MENU*) in Thread Mode
	  if (dynamic_schema_requested){
		  dynamic_schema_requested = 0;
		  menu_export_dynamic_schema();
	  }

	  // Check for dynamic library query request (!LIB*) in Thread Mode
	  if (dynamic_lib_requested){
		  dynamic_lib_requested = 0;
		  lib_export_dynamic_schema();
	  }

	  // Check for dynamic library action requests
	  if (cmd_lib_run_slot >= 0){
		  uint8_t s = (uint8_t)cmd_lib_run_slot;
		  cmd_lib_run_slot = -1;
		  CDC_Printf("!LIB:RUN:OK:%u\r\n", (unsigned int)s);
		  lib_load_and_run(s);
	  }
	  if (cmd_lib_dump_slot >= 0){
		  uint8_t s = (uint8_t)cmd_lib_dump_slot;
		  cmd_lib_dump_slot = -1;
		  lib_dump_program(s);
	  }
	  if (cmd_lib_del_slot >= 0){
		  uint8_t s = (uint8_t)cmd_lib_del_slot;
		  cmd_lib_del_slot = -1;
		  lib_delete_program(s);
		  CDC_Printf("!LIB:DEL:OK:%u\r\n", (unsigned int)s);
		  dynamic_lib_requested = 1;
	  }

	  // Check for restore factory demo request in Thread Mode
	  if (cmd_restore_requested){
		  cmd_restore_requested = 0;
		  CDC_Print("\r\nRestoring Default Demo to Flash...\r\n");
		  flashDefaultLogoProgram();
	  }

	  // Check for DFU jump request (executed in Thread Mode outside interrupt)
	  if (dfu_requested){
		  dfu_requested = 0;
		  Execute_DFU_Jump();
	  }

	  // Check for soft-processor reset request
	  if (reset_requested){
		  reset_requested = 0;
		  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
		  HAL_GPIO_WritePin(BF_RST_GPIO_Port, BF_RST_Pin, GPIO_PIN_RESET);
		  HAL_Delay(10);
		  HAL_GPIO_WritePin(BF_RST_GPIO_Port, BF_RST_Pin, GPIO_PIN_SET);
		  HAL_Delay(60);
		  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
		  manual_step_ticks_pending = 0;
		  serial_inbox_head = 0;
		  serial_inbox_tail = 0;
		  serial_input = 0;
		  set_freq(freq);
		  CDC_Printf("\r\n[bf_\xC2\xB5P reset] %s\r\n", get_freq_name(freq));
		  if (cfg_manual_step_enabled){
			  CDC_Printf("[Manual Step Mode Active: Press %s to step %s]\r\n",
			             get_step_key_name(cfg_manual_step_key), get_step_ticks_name(cfg_manual_step_ticks));
		  }
	  }

	  // 1. Rock-solid debounced button state machine (15ms stable window to filter mechanical chatter)
	  uint8_t btn_raw = HAL_GPIO_ReadPin(BRD_RST_GPIO_Port, BRD_RST_Pin);

	  if (btn_raw != btn_last_sample){
		  btn_last_sample = btn_raw;
		  btn_last_stable_tick = HAL_GetTick();
	  }
	  else if ((HAL_GetTick() - btn_last_stable_tick) >= 15){
		  if ((btn_raw == GPIO_PIN_RESET) && !btn_is_pressed){
			  // Button officially PRESSED (held down)
			  btn_is_pressed = 1;
			  btn_press_tick = HAL_GetTick();
			  btn_held_1s = 0;
			  btn_held_3s = 0;
			  btn_held_8s = 0;
			  // Hold FPGA in reset while physical button is held down
			  HAL_GPIO_WritePin(BF_RST_GPIO_Port, BF_RST_Pin, GPIO_PIN_RESET);
		  }
		  else if ((btn_raw == GPIO_PIN_SET) && btn_is_pressed){
			  // Button officially RELEASED
			  btn_is_pressed = 0;
			  uint32_t press_duration = HAL_GetTick() - btn_press_tick;

			  if (btn_held_8s || (press_duration >= 8000)){
				  // Held >= 8s: Restore Default Brainfuino ASCII Logo Demo!
				  flashDefaultLogoProgram();
			  }
			  else if (btn_held_3s || (press_duration >= 3000)){
				  // Held >= 3s: Enter Interactive Configuration Menu
				  menu_enter();
			  }
			  else if (btn_held_1s || (press_duration >= 1000)){
				  // Held >= 1s: Enter Program Mode
				  state = STATE_PROGRAM;
				  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
				  HAL_GPIO_WritePin(BF_RST_GPIO_Port, BF_RST_Pin, GPIO_PIN_RESET); // Hold soft-processor in reset

				  prog_rx_count = 0;
				  prog_total_received = 0;
				  prog_raw_received = 0;
				  prog_flash_addr = 0;
				  prog_active = 0;
				  prog_is_streaming = 0;
				  staging_head = 0;
				  staging_tail = 0;
				  stream_init_done = 0;

				  CDC_Print("\r\n\r\n=== BRAINFUINO PROGRAM MODE ===\r\nPaste Brainfuck code now (up to 256 kB)...\r\n");
				  CDC_Resume_Rx();
			  }
			  else if (press_duration >= 15){
				  // Short tap (< 1s)
				  if (state == STATE_CONFIG){
					  // In Config Menu: short press exits menu and resumes program
					  menu_exit();
				  }
				  else if (state == STATE_PROGRAM){
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
					  serial_inbox_head = 0;
					  serial_inbox_tail = 0;
					  serial_input = 0;
					  state = STATE_RUN;
					  CDC_Print("\r\n[Running program]\r\n");
				  }
				  else {
					  // In Run Mode: short press resets the running soft-processor and blips the LED!
					  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
					  HAL_GPIO_WritePin(BF_RST_GPIO_Port, BF_RST_Pin, GPIO_PIN_RESET);
					  HAL_Delay(10);
					  HAL_GPIO_WritePin(BF_RST_GPIO_Port, BF_RST_Pin, GPIO_PIN_SET);
					  HAL_Delay(60);
					  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
					  manual_step_ticks_pending = 0;
					  serial_inbox_head = 0;
					  serial_inbox_tail = 0;
					  serial_input = 0;
					  set_freq(freq);
					  CDC_Printf("\r\n[bf_\xC2\xB5P reset] %s\r\n", get_freq_name(freq));
					  if (cfg_manual_step_enabled){
						  CDC_Printf("[Manual Step Mode Active: Press %s to step %s]\r\n",
						             get_step_key_name(cfg_manual_step_key), get_step_ticks_name(cfg_manual_step_ticks));
					  }
				  }
			  }
			  btn_held_1s = 0;
			  btn_held_3s = 0;
			  btn_held_8s = 0;
			  if (state == STATE_RUN){
				  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
			  }
		  }
	  }

	  if (btn_is_pressed){
		  // Button is currently being held down
		  uint32_t hold_time = HAL_GetTick() - btn_press_tick;
		  if (hold_time >= 8000){
			  btn_held_8s = 1;
			  // Rapid Red LED strobe (toggle every 50ms) at 8s mark for Factory Demo Reset
			  if ((hold_time / 50) % 2){
				  LED_GPIO_Port->BSRR = LED_Pin;
			  } else {
				  LED_GPIO_Port->BRR = LED_Pin;
			  }
		  }
		  else if (hold_time >= 3000){
			  btn_held_3s = 1;
			  // Visual indication: smooth pulse (period 600ms) at 3s mark for Settings Menu threshold
			  led_update_pulse(600);
		  }
		  else if (hold_time >= 1000){
			  btn_held_1s = 1;
			  // Visual indication: Red LED turns ON solid at 1.0s (Program Mode threshold)
			  LED_GPIO_Port->BSRR = LED_Pin;
		  }
		  else {
			  // 0 to 1s hold: in config mode, keep solid ON for immediate tactile feedback
			  if (state == STATE_CONFIG){
				  LED_GPIO_Port->BSRR = LED_Pin;
			  }
		  }
	  }

	  // 2. Interactive Configuration Menu handling (Thread Mode)
	  if (state == STATE_CONFIG){
		  if (menu_exit_requested){
			  menu_exit_requested = 0;
			  menu_exit();
		  }
		  else if (lib_start_add_requested){
			  lib_start_add_requested = 0;
			  lib_start_add_program();
		  }
		  else if (lib_delete_pending_slot >= 0){
			  uint8_t slot = (uint8_t)lib_delete_pending_slot;
			  lib_delete_pending_slot = -1;
			  lib_delete_program(slot);
			  CDC_Printf("\r\n[Deleted Slot %02d]\r\n", slot);
			  HAL_Delay(300);
			  menu_render();
		  }
		  else if (lib_load_pending_slot >= 0){
			  uint8_t slot = (uint8_t)lib_load_pending_slot;
			  lib_load_pending_slot = -1;
			  lib_load_and_run(slot);
		  }
		  else if (lib_dump_pending_slot >= 0){
			  uint8_t slot = (uint8_t)lib_dump_pending_slot;
			  lib_dump_pending_slot = -1;
			  lib_dump_program(slot);
		  }
		  else if (menu_action_pending >= 0){
			  uint8_t act = (uint8_t)menu_action_pending;
			  int8_t dir = menu_action_dir;
			  menu_action_pending = -1;
			  menu_execute_action(act, dir);
			  if (state == STATE_CONFIG){
				  menu_render();
			  }
		  }
		  else if (menu_needs_render){
			  menu_needs_render = 0;
			  menu_render();
		  }
		  else if (!btn_is_pressed){
			  // Continuous smooth LED breathing/pulsing while in Config Menu (only when actively in menu)
			  led_update_pulse(1200);
		  }
	  }

	  // 2.5 Library Add Program handling (Thread Mode)
	  if (state == STATE_LIB_ADD){
		  if (lib_add_substate == LIB_ADD_PASTE_PROMPT){
			  CDC_Printf("\r\nName: %s\r\n", lib_name_buf);
			  CDC_Print("Preparing scratchpad... ");
			  initROMProgramming();
			  eraseROMFast();
			  CDC_Print("Ready.\r\n");
			  CDC_Print("Paste Brainfuck code now (press Enter or pause 100ms when done)...\r\n");
			  lib_code_size = 0;
			  lib_raw_rx_size = 0;
			  lib_add_last_rx_tick = HAL_GetTick();
			  lib_add_substate = LIB_ADD_PASTE;
		  }
		  else if (lib_add_substate == LIB_ADD_PASTE && lib_code_size > 0){
			  if (HAL_GetTick() - lib_add_last_rx_tick >= PROG_IDLE_TIMEOUT_MS){
				  CDC_Printf("\r\nReceived %lu valid bytes.\r\n", (unsigned long)lib_code_size);
				  if (cfg_prune_in_library && lib_raw_rx_size > 0){
					  uint32_t saved = (lib_raw_rx_size > lib_code_size) ? (lib_raw_rx_size - lib_code_size) : 0;
					  uint32_t pct = (saved * 100) / lib_raw_rx_size;
					  uint32_t pct_dec = (saved * 1000 / lib_raw_rx_size) % 10;
					  CDC_Printf("[Prune Stats: Received %lu B | Kept %lu B | Saved %lu.%lu%% non-BF comments]\r\n",
					             (unsigned long)lib_raw_rx_size, (unsigned long)lib_code_size, (unsigned long)pct, (unsigned long)pct_dec);
				  }

				  uint32_t used_bytes = lib_get_total_used_bytes();
				  uint32_t free_bytes = (used_bytes < LIBRARY_POOL_SIZE) ? (LIBRARY_POOL_SIZE - used_bytes) : 0;

				  if (lib_code_size > LIBRARY_POOL_SIZE){
					  CDC_Printf("\r\n[ERROR: Program size (%lu B) exceeds total Library capacity (76 kB)]\r\n\r\n", (unsigned long)lib_code_size);
					  HAL_Delay(2000);
					  state = STATE_CONFIG;
					  menu_page = MENU_PAGE_LIBRARY;
					  menu_needs_render = 1;
				  }
				  else if (lib_code_size > free_bytes){
					  CDC_Printf("\r\n[Warning: Program requires %lu B, but only %lu B free (need %lu B more)]\r\n",
					             (unsigned long)lib_code_size, (unsigned long)free_bytes, (unsigned long)(lib_code_size - free_bytes));
					  CDC_Print("Active programs that can be deleted to make room:\r\n");
					  for (uint8_t s = 1; s < MAX_LIB_SLOTS; s++){
						  if (lib_toc[s].status == 0x01){
							  CDC_Printf("  Slot %02d: %-15s [%lu B]\r\n", s, lib_toc[s].name, (unsigned long)lib_toc[s].size);
						  }
					  }
					  CDC_Print("Type slot number to delete, or [0] to cancel: ");
					  lib_capacity_delete_slot = -1;
					  lib_add_substate = LIB_ADD_CAPACITY_WARN;
				  }
				  else {
					  if (lib_auto_verify == 2){
						  lib_auto_verify = 0;
						  CDC_Print("\r\nSaving directly to Library (No test requested)...\r\n");
						  lib_add_substate = LIB_ADD_COMMIT;
					  }
					  else if (lib_auto_verify == 1){
						  lib_auto_verify = 0;
						  CDC_Print("\r\n\r\nRunning on Brainfuino...\r\n");
						  CDC_Print("!LIB:TEST:START:10\r\n");
						  lib_verify_requested = 1;
						  if (cfg_append_endless_loop){
							  writeROMFast(lib_code_size, '[');
							  writeROMFast(lib_code_size + 1, '-');
							  writeROMFast(lib_code_size + 2, ']');
							  writeROMFast(lib_code_size + 3, '+');
							  writeROMFast(lib_code_size + 4, '[');
							  writeROMFast(lib_code_size + 5, ']');
						  }
						  initROMNormal();
						  wait(1000);
						  HAL_GPIO_WritePin(OE_GPIO_Port, OE_Pin, GPIO_PIN_RESET);
						  wait(1000);
						  set_freq(freq);
						  head = tail = 0;
						  mco_throttled = 0;
						  HAL_GPIO_WritePin(BF_RST_GPIO_Port, BF_RST_Pin, GPIO_PIN_RESET);
						  HAL_Delay(10);
						  HAL_GPIO_WritePin(BF_RST_GPIO_Port, BF_RST_Pin, GPIO_PIN_SET);
						  lib_verify_start_tick = HAL_GetTick();
						  lib_add_substate = LIB_ADD_VERIFYING;
					  } else {
						  CDC_Print("Run on Brainfuino to verify before saving? [Y/n]: ");
						  lib_add_substate = LIB_ADD_VERIFY_PROMPT;
					  }
				  }
			  }
		  }
		  else if (lib_add_substate == LIB_ADD_CAPACITY_WARN){
			  if (lib_capacity_delete_slot >= 0){
				  uint8_t del_s = (uint8_t)lib_capacity_delete_slot;
				  lib_capacity_delete_slot = -1;
				  lib_delete_program(del_s);
				  CDC_Printf("\r\n[Deleted Slot %02d]\r\n", del_s);
				  uint32_t used_bytes = lib_get_total_used_bytes();
				  uint32_t free_bytes = (used_bytes < LIBRARY_POOL_SIZE) ? (LIBRARY_POOL_SIZE - used_bytes) : 0;
				  if (lib_code_size <= free_bytes){
					  CDC_Printf("[Sufficient space cleared: %lu B free. Proceeding!]\r\n\r\n", (unsigned long)free_bytes);
					  CDC_Print("Run on Brainfuino to verify before saving? [Y/n]: ");
					  lib_add_substate = LIB_ADD_VERIFY_PROMPT;
				  } else {
					  CDC_Printf("Still need %lu B more. Type another slot to delete (or 0 to cancel): ",
					             (unsigned long)(lib_code_size - free_bytes));
				  }
			  }
		  }
		  else if (lib_add_substate == LIB_ADD_VERIFY_PROMPT){
			  if (lib_verify_choice_pending){
				  lib_verify_choice_pending = 0;
				  if (lib_verify_choice == 0){
					  CDC_Print("No\r\nSaving directly to Library...\r\n");
					  lib_verify_requested = 0;
					  lib_add_substate = LIB_ADD_COMMIT;
				  }
				  else {
					  CDC_Print("Yes\r\n\r\nRunning on Brainfuino...\r\n");
					  CDC_Print("!LIB:TEST:START:10\r\n");
					  CDC_Print("Press RESET button (or type !RST) within 10s to abort.\r\n");
					  CDC_Print("Auto-saving in 10s (or press Enter to save now)...\r\n\r\n");
					  lib_verify_requested = 1;
					  if (cfg_append_endless_loop){
						  writeROMFast(lib_code_size, '[');
						  writeROMFast(lib_code_size + 1, '-');
						  writeROMFast(lib_code_size + 2, ']');
						  writeROMFast(lib_code_size + 3, '+');
						  writeROMFast(lib_code_size + 4, '[');
						  writeROMFast(lib_code_size + 5, ']');
					  }
					  initROMNormal();
					  wait(1000);
					  HAL_GPIO_WritePin(OE_GPIO_Port, OE_Pin, GPIO_PIN_RESET);
					  wait(1000);
					  set_freq(freq);
					  head = tail = 0;
					  mco_throttled = 0;
					  HAL_GPIO_WritePin(BF_RST_GPIO_Port, BF_RST_Pin, GPIO_PIN_RESET);
					  HAL_Delay(10);
					  HAL_GPIO_WritePin(BF_RST_GPIO_Port, BF_RST_Pin, GPIO_PIN_SET);
					  lib_verify_start_tick = HAL_GetTick();
					  lib_add_substate = LIB_ADD_VERIFYING;
				  }
			  }
		  }
		  else if (lib_add_substate == LIB_ADD_VERIFYING){
			  if (reset_requested || btn_is_pressed){
				  reset_requested = 0;
				  HAL_GPIO_WritePin(BF_RST_GPIO_Port, BF_RST_Pin, GPIO_PIN_RESET);
				  flashDefaultLogoProgram();
				  CDC_Print("\r\n\r\n[Upload Cancelled: Program was NOT saved to Library]\r\n\r\n");
				  CDC_Print("!LIB:TEST:CANCEL\r\n");
				  HAL_Delay(1200);
				  state = STATE_CONFIG;
				  menu_page = MENU_PAGE_LIBRARY;
				  menu_needs_render = 1;
			  }
			  else if (lib_verify_early_commit){
				  lib_verify_early_commit = 0;
				  HAL_GPIO_WritePin(BF_RST_GPIO_Port, BF_RST_Pin, GPIO_PIN_RESET);
				  CDC_Print("\r\n\r\n[Verification complete: Saving to Library...]\r\n");
				  lib_add_substate = LIB_ADD_COMMIT;
			  }
			  else if (HAL_GetTick() - lib_verify_start_tick >= 10000){
				  HAL_GPIO_WritePin(BF_RST_GPIO_Port, BF_RST_Pin, GPIO_PIN_RESET);
				  CDC_Print("\r\n\r\n[10s Verification Elapsed: Auto-saving to Library...]\r\n");
				  lib_add_substate = LIB_ADD_COMMIT;
			  }
		  }
		  else if (lib_add_substate == LIB_ADD_COMMIT){
			  uint8_t slot = lib_save_program(lib_name_buf, lib_code_size, cfg_prune_in_library);
			  if (slot > 0){
				  CDC_Printf("\r\n[SUCCESS: Saved to Slot %02d: '%s' (%lu bytes)]\r\n\r\n", slot, lib_name_buf, (unsigned long)lib_code_size);
				  CDC_Printf("!LIB:TEST:DONE:%u\r\n", (unsigned int)slot);
			  } else {
				  CDC_Print("\r\n[ERROR: Failed to save program to Library partition]\r\n\r\n");
				  CDC_Print("!LIB:TEST:ERROR\r\n");
			  }
			  HAL_Delay(1500);
			  state = STATE_CONFIG;
			  menu_page = MENU_PAGE_LIBRARY;
			  menu_needs_render = 1;
			  dynamic_lib_requested = 1;
		  }
	  }

	  // 3. Program Mode flashing handling
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
				  if (cfg_append_endless_loop && (prog_flash_addr + 6 <= FLASH_CAPACITY)){
					  writeROMFast(prog_flash_addr++, '[');
					  writeROMFast(prog_flash_addr++, '-');
					  writeROMFast(prog_flash_addr++, ']');
					  writeROMFast(prog_flash_addr++, '+');
					  writeROMFast(prog_flash_addr++, '[');
					  writeROMFast(prog_flash_addr++, ']');
				  }

				  uint32_t written = (prog_flash_addr > FLASH_CAPACITY) ? FLASH_CAPACITY : prog_flash_addr;
				  CDC_Printf("\r\nFinished! Wrote %lu bytes total.\r\n", written);

				  if (cfg_auto_reset_after_pgm){
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
			  if (cfg_prune_on_paste && prog_raw_received > 0){
				  uint32_t saved = (prog_raw_received > prog_rx_count) ? (prog_raw_received - prog_rx_count) : 0;
				  uint32_t pct = (saved * 100) / prog_raw_received;
				  uint32_t pct_dec = (saved * 1000 / prog_raw_received) % 10;
				  CDC_Printf("\r\n[Prune Stats: Received %lu B | Kept %lu B | Saved %lu.%lu%% non-BF comments]\r\n",
				             (unsigned long)prog_raw_received, (unsigned long)prog_rx_count, (unsigned long)pct, (unsigned long)pct_dec);
			  }
			  // Program <= 4 kB: paste complete! Flash via unified engine
			  flashBufferToROM(prog_buffer, prog_rx_count, NULL, cfg_append_endless_loop, cfg_auto_reset_after_pgm);

			  prog_active = 0;
			  prog_rx_count = 0;
			  prog_total_received = 0;
			  prog_raw_received = 0;
			  prog_flash_addr = 0;
			  prog_is_streaming = 0;
			  staging_head = 0;
			  staging_tail = 0;
			  stream_init_done = 0;
			  CDC_Resume_Rx();
		  }
	  }

	  // Check if throttled FPGA clock can resume after buffer drained
	  if (mco_throttled){
		  uint16_t used = (tail >= head) ? (tail - head) : (OUTBOX_CAPACITY - (head - tail));
		  if (used < (OUTBOX_CAPACITY / 2)){
			  mco_throttled = 0;
			  if (active_is_tim1){
				  if (!cfg_manual_step_enabled){
					  TIM1->CR1 |= TIM_CR1_CEN;
				  }
			  } else {
				  if (!cfg_manual_step_enabled){
					  RCC->CFGR = (RCC->CFGR & ~(RCC_CFGR_MCO | RCC_CFGR_MCOPRE)) | active_mco_cfg;
				  }
			  }
		  }
	  }

	  // Send data out to host computer
	  if(((state == STATE_RUN) || (state == STATE_LIB_ADD && lib_add_substate == LIB_ADD_VERIFYING)) && (head != tail) && !TxBusy()){
		  uint32_t cur_tail = tail;
		  leaving = head;

		  if (speed_overlay_remaining > 0){
			  uint8_t c = outbox[head];
			  if (c == '\r' || c == '\n'){
				  // FPGA produced newline before speed overlay was fully overwritten:
				  // Transmit \x1b[K to erase rest of line, plus the newline byte(s), in one single USB packet!
				  uint8_t nl_pkt[16];
				  uint8_t nlen = 0;
				  nl_pkt[nlen++] = 0x1B;
				  nl_pkt[nlen++] = '[';
				  nl_pkt[nlen++] = 'K';

				  while ((head != cur_tail) && (outbox[head] == '\r' || outbox[head] == '\n') && (nlen < sizeof(nl_pkt))){
					  nl_pkt[nlen++] = outbox[head];
					  head++;
					  if (head >= OUTBOX_CAPACITY) head = 0;
				  }

				  CDC_Transmit_FS(nl_pkt, nlen);
				  speed_overlay_remaining = 0;
			  }
			  else {
				  // Non-newline bytes: scan up to speed_overlay_remaining non-newlines
				  uint32_t avail = (head < cur_tail) ? (cur_tail - head) : (OUTBOX_CAPACITY - head);
				  uint32_t chunk = 0;
				  while (chunk < avail && chunk < speed_overlay_remaining &&
				         outbox[head + chunk] != '\r' && outbox[head + chunk] != '\n'){
					  chunk++;
				  }
				  if (chunk == 0) chunk = 1;
				  CDC_Transmit_FS(outbox + head, chunk);
				  head += chunk;
				  if (head >= OUTBOX_CAPACITY) head = 0;
				  if (chunk >= speed_overlay_remaining) speed_overlay_remaining = 0;
				  else speed_overlay_remaining -= chunk;
			  }
		  }
		  else {
			  if(head < cur_tail){
				  if( (cur_tail - head) < 60 ){ // easy case
					  CDC_Transmit_FS(outbox+head, cur_tail - head);
					  head = cur_tail;
				  }
				  else{ 				// chunk it
					  CDC_Transmit_FS(outbox+head, 60);
					  head += 60;
				  }
			  }
			  else{
				  if(head >= (OUTBOX_CAPACITY - 60)){ // easy case
					  CDC_Transmit_FS(outbox+head, OUTBOX_CAPACITY - head);
					  head = 0;
				  }
				  else{ 				// chunk it
					  CDC_Transmit_FS(outbox+head, 60);
					  head += 60;
				  }
			  }
		  }

		  if (mco_throttled){
			  uint16_t used = (tail >= head) ? (tail - head) : (OUTBOX_CAPACITY - (head - tail));
			  if (used < (OUTBOX_CAPACITY / 2)){
				  mco_throttled = 0;
				  if (active_is_tim1){
					  if (!cfg_manual_step_enabled){
						  TIM1->CR1 |= TIM_CR1_CEN;
					  }
				  } else {
					  if (!cfg_manual_step_enabled){
						  RCC->CFGR = (RCC->CFGR & ~(RCC_CFGR_MCO | RCC_CFGR_MCOPRE)) | active_mco_cfg;
					  }
				  }
			  }
		  }
	  }

	  // update hardware input (digital or analog)
	  __disable_irq();
	  if((state == STATE_RUN) && !serial_input){
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

	  // 4. Delayed Flash save execution (wear-leveling debouncer: 3-second idle)
	  if (settings_dirty && ((HAL_GetTick() - settings_dirty_tick) >= 3000)){
		  settings_dirty = 0;
		  settings_save();
	  }

	  // 5. Manual Step Mode clock pulse draining engine
	  if ((state == STATE_RUN) && cfg_manual_step_enabled && (manual_step_ticks_pending > 0)){
		  __disable_irq();
		  uint32_t to_step = manual_step_ticks_pending;
		  manual_step_ticks_pending = 0;
		  __enable_irq();

		  if (active_is_tim1){
			  // In TIM1 mode (10 Hz - 50 kHz):
			  // Enable TIM1 counter to emit PWM pulses
			  TIM1->CR1 |= TIM_CR1_CEN;
			  for (uint32_t s = 0; s < to_step; s++){
				  TIM1->SR &= ~TIM_SR_UIF;
				  while (!(TIM1->SR & TIM_SR_UIF)){
					  // Drain any characters generated during this step to CDC
					  if ((head != tail) && !TxBusy()){
						  uint32_t cur_tail = tail;
						  if (head < cur_tail){
							  uint16_t sz = (cur_tail - head < 60) ? (cur_tail - head) : 60;
							  CDC_Transmit_FS(outbox + head, sz);
							  head += sz;
						  } else {
							  uint16_t sz = (OUTBOX_CAPACITY - head < 60) ? (OUTBOX_CAPACITY - head) : 60;
							  CDC_Transmit_FS(outbox + head, sz);
							  head = (head + sz >= OUTBOX_CAPACITY) ? 0 : head + sz;
						  }
					  }
				  }
			  }
			  TIM1->CR1 &= ~TIM_CR1_CEN;
		  } else {
			  // In MCO mode (62.5 kHz - 12 MHz):
			  // Clock is generated directly by MCU MCO hardware divider.
			  RCC->CFGR = (RCC->CFGR & ~(RCC_CFGR_MCO | RCC_CFGR_MCOPRE)) | active_mco_cfg;

			  uint32_t cur_freq_hz = freq_table[freq].freq_hz;
			  uint32_t cpu_cycles_per_clk = 48000000UL / cur_freq_hz;
			  if (cpu_cycles_per_clk == 0) cpu_cycles_per_clk = 1;

			  if (to_step <= 100){
				  for (uint32_t s = 0; s < to_step; s++){
					  uint32_t loops = (cpu_cycles_per_clk >= 4) ? (cpu_cycles_per_clk / 4) : 1;
					  for (volatile uint32_t d = 0; d < loops; d++){
						  __NOP();
					  }
					  if ((head != tail) && !TxBusy()){
						  uint32_t cur_tail = tail;
						  if (head < cur_tail){
							  uint16_t sz = (cur_tail - head < 60) ? (cur_tail - head) : 60;
							  CDC_Transmit_FS(outbox + head, sz);
							  head += sz;
						  } else {
							  uint16_t sz = (OUTBOX_CAPACITY - head < 60) ? (OUTBOX_CAPACITY - head) : 60;
							  CDC_Transmit_FS(outbox + head, sz);
							  head = (head + sz >= OUTBOX_CAPACITY) ? 0 : head + sz;
						  }
					  }
				  }
			  } else {
				  uint32_t total_us = (uint32_t)(((uint64_t)to_step * 1000000ULL) / cur_freq_hz);
				  if (total_us == 0) total_us = 1;

				  uint32_t remaining_us = total_us;
				  while (remaining_us > 0){
					  uint32_t chunk = (remaining_us > 100) ? 100 : remaining_us;
					  uint32_t loops = chunk * 6;
					  for (volatile uint32_t d = 0; d < loops; d++){
						  __NOP();
					  }
					  remaining_us -= chunk;

					  if ((head != tail) && !TxBusy()){
						  uint32_t cur_tail = tail;
						  if (head < cur_tail){
							  uint16_t sz = (cur_tail - head < 60) ? (cur_tail - head) : 60;
							  CDC_Transmit_FS(outbox + head, sz);
							  head += sz;
						  } else {
							  uint16_t sz = (OUTBOX_CAPACITY - head < 60) ? (OUTBOX_CAPACITY - head) : 60;
							  CDC_Transmit_FS(outbox + head, sz);
							  head = (head + sz >= OUTBOX_CAPACITY) ? 0 : head + sz;
						  }
					  }
				  }
			  }

			  RCC->CFGR &= ~RCC_CFGR_MCO;
		  }
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
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{
  __HAL_RCC_TIM1_CLK_ENABLE();

  TIM1->CR1 = 0;
  TIM1->PSC = 479;
  TIM1->ARR = 1;
  TIM1->CCR1 = 1;
  TIM1->CCMR1 = (6 << TIM_CCMR1_OC1M_Pos) | TIM_CCMR1_OC1PE; // PWM mode 1, preload enable
  TIM1->CCER = TIM_CCER_CC1E;                                // Enable CH1 output (PA8)
  TIM1->BDTR = TIM_BDTR_MOE;                                 // Main Output Enable
  TIM1->CR1 = TIM_CR1_ARPE;                                  // Auto-reload preload enable
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
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
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
		serial_inbox_head = 0;
		serial_inbox_tail = 0;
		serial_input = 0;
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
	                 cfg_append_endless_loop,
	                 cfg_auto_reset_after_pgm);

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
