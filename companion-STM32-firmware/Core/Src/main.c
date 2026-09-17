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

#define MENU_ITEM_COUNT          9

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
volatile uint32_t head;
uint32_t leaving;
volatile uint32_t tail;
uint32_t tail_temp;
volatile uint32_t active_mco_cfg = 0;
volatile uint8_t  mco_throttled = 0;

// Runtime configuration settings (modifiable in interactive configuration menu)
uint8_t  cfg_auto_prog_run_mode   = 1;   // 1 = Auto-enter program mode if code paste detected in RUN mode
uint32_t cfg_auto_prog_min_bytes  = 16;  // Minimum packet size to trigger upload (safe against 3-5 byte ANSI escape keys)
uint8_t  cfg_auto_reset_after_pgm = 1;   // 1 = Automatically reset and launch program after write finishes
uint8_t  cfg_append_endless_loop  = 1;   // 1 = Automatically append '[-]+[]' infinite loop at EOF to halt PC cleanly
uint8_t  cfg_speed_hotkey_mode    = SPEED_HOTKEY_PGUP_PGDN;

int8_t   menu_cursor = 0;
volatile uint8_t menu_needs_render = 0;
volatile uint8_t menu_exit_requested = 0;
volatile int8_t  menu_action_pending = -1;

volatile uint8_t dfu_requested;

// Button debounce and hold tracking
uint8_t btn_debounced;
uint8_t btn_last_raw;
uint32_t btn_raw_change_tick;
uint32_t btn_press_tick;
uint8_t btn_held_3s;
uint8_t btn_held_6s;
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
void menu_enter(void);
void menu_exit(void);
void menu_render(void);
void menu_execute_action(uint8_t item);
void led_update_pulse(uint32_t period_ms);
void settings_load(void);
void settings_save(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin){
	if (GPIO_Pin == GPIO_PIN_1){  // InStrobe
		HAL_GPIO_WritePin(BF_INCMG_GPIO_Port, BF_INCMG_Pin, GPIO_PIN_RESET);
		serial_input = 0;
	}
}

typedef struct {
	const char *name;
	uint32_t mco_cfg;
} FreqConfig;

static const FreqConfig freq_table[] = {
	{ "62.5 kHz", RCC_MCO1SOURCE_HSI   | RCC_MCODIV_128 },
	{ "125 kHz",  RCC_MCO1SOURCE_HSI   | RCC_MCODIV_64  },
	{ "250 kHz",  RCC_MCO1SOURCE_HSI   | RCC_MCODIV_32  },
	{ "500 kHz",  RCC_MCO1SOURCE_HSI   | RCC_MCODIV_16  },
	{ "750 kHz",  RCC_MCO1SOURCE_HSI48 | RCC_MCODIV_64  },
	{ "1 MHz",    RCC_MCO1SOURCE_HSI   | RCC_MCODIV_8   },
	{ "1.5 MHz",  RCC_MCO1SOURCE_HSI48 | RCC_MCODIV_32  },
	{ "2 MHz",    RCC_MCO1SOURCE_HSI   | RCC_MCODIV_4   },
	{ "3 MHz",    RCC_MCO1SOURCE_HSI48 | RCC_MCODIV_16  },
	{ "4 MHz",    RCC_MCO1SOURCE_HSI   | RCC_MCODIV_2   },
	{ "6 MHz",    RCC_MCO1SOURCE_HSI48 | RCC_MCODIV_8   },
	{ "8 MHz",    RCC_MCO1SOURCE_HSI   | RCC_MCODIV_1   },
	{ "12 MHz",   RCC_MCO1SOURCE_HSI48 | RCC_MCODIV_4   },
};
#define FREQ_COUNT ((uint8_t)(sizeof(freq_table) / sizeof(freq_table[0])))
#define DEFAULT_FREQ_INDEX 3 // 500 kHz

void set_freq(uint8_t idx){
	if (idx >= FREQ_COUNT) idx = DEFAULT_FREQ_INDEX;
	freq = idx;
	active_mco_cfg = freq_table[idx].mco_cfg;
	HAL_RCC_MCOConfig(RCC_MCO, active_mco_cfg & RCC_CFGR_MCO, active_mco_cfg & RCC_CFGR_MCOPRE);
}

const char *get_freq_name(uint8_t idx){
	if (idx >= FREQ_COUNT) return "Unknown";
	return freq_table[idx].name;
}

void speed_step_up(void){
	if (freq + 1 < FREQ_COUNT){
		set_freq(freq + 1);
		// Transient overlay: save cursor, print dimmed clock status, restore cursor so BF code overwrites it naturally
		CDC_Printf("\x1b[s\x1b[2m[Clock: %s]\x1b[0m\x1b[u", get_freq_name(freq));
		HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
		for (volatile int i = 0; i < 60000; i++);
		HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
	}
}

void speed_step_down(void){
	if (freq > 0){
		set_freq(freq - 1);
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

void menu_render(void){
	// Clear screen and home cursor (VT100 / ANSI)
	CDC_Print("\x1b[2J\x1b[H\r\n");
	CDC_Print("+-----------------------------------------------+\r\n");
	CDC_Print("|         BRAINFUINO CONFIGURATION MENU         |\r\n");
	CDC_Print("+-----------------------------------------------+\r\n");
	CDC_Print("|  Use [Up/Down] & [Enter], or type [1-8, 0]    |\r\n");
	CDC_Print("+-----------------------------------------------+\r\n");

	for (int i = 0; i < MENU_ITEM_COUNT; i++){
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
				label = "6. Run Mode Speed Hotkeys ";
				{
					const char *hname = "NONE";
					if (cfg_speed_hotkey_mode == SPEED_HOTKEY_PGUP_PGDN) hname = "PgUp/Dn";
					else if (cfg_speed_hotkey_mode == SPEED_HOTKEY_UP_DOWN) hname = "Up/Down";
					else if (cfg_speed_hotkey_mode == SPEED_HOTKEY_PLUS_MINUS) hname = "+ / -";
					snprintf(val_str, sizeof(val_str), "[ %-8s ]", hname);
				}
				break;
			case 6:
				label = "7. Restore Default Demo   ";
				snprintf(val_str, sizeof(val_str), "[ RESTORE  ]");
				break;
			case 7:
				label = "8. Reboot to USB DFU      ";
				snprintf(val_str, sizeof(val_str), "[  REBOOT  ]");
				break;
			case 8:
				label = "0. Save & Exit            ";
				snprintf(val_str, sizeof(val_str), "[   EXIT   ]");
				break;
		}

		if (i == menu_cursor){
			// Highlighted row: Inverted video (\x1b[7m) + cursor indicators '> ' and ' <'
			CDC_Printf("| \x1b[7m> %s : %-12s <\x1b[0m |\r\n", label, val_str);
		} else {
			CDC_Printf("|   %s : %-12s   |\r\n", label, val_str);
		}
	}

	CDC_Print("+-----------------------------------------------+\r\n");
	CDC_Print("|  Hardware: STM32F072 | Parallel ROM: 256 kB   |\r\n");
	CDC_Print("+-----------------------------------------------+\r\n");
	CDC_Print("Select option [0-8] or use arrows + Enter: ");
}

void menu_execute_action(uint8_t item){
	switch(item){
		case 0:
			cfg_auto_prog_run_mode = !cfg_auto_prog_run_mode;
			break;
		case 1:
			if (cfg_auto_prog_min_bytes == 4) cfg_auto_prog_min_bytes = 8;
			else if (cfg_auto_prog_min_bytes == 8) cfg_auto_prog_min_bytes = 16;
			else if (cfg_auto_prog_min_bytes == 16) cfg_auto_prog_min_bytes = 32;
			else if (cfg_auto_prog_min_bytes == 32) cfg_auto_prog_min_bytes = 64;
			else cfg_auto_prog_min_bytes = 4;
			break;
		case 2:
			cfg_auto_reset_after_pgm = !cfg_auto_reset_after_pgm;
			break;
		case 3:
			cfg_append_endless_loop = !cfg_append_endless_loop;
			break;
		case 4:
			set_freq((freq + 1) % FREQ_COUNT);
			break;
		case 5:
			cfg_speed_hotkey_mode = (cfg_speed_hotkey_mode + 1) % 4;
			break;
		case 6:
			CDC_Print("\r\nRestoring Default Demo to Flash...\r\n");
			flashDefaultLogoProgram();
			break;
		case 7:
			CDC_Print("\r\nRebooting to STM32 USB DFU Bootloader...\r\n");
			HAL_Delay(200);
			dfu_requested = 1;
			state = STATE_RUN;
			break;
		case 8:
			menu_exit();
			break;
		default:
			break;
	}
}

#define SETTINGS_FLASH_ADDR   0x0801F800UL
#define SETTINGS_MAGIC        0xBF072C01UL

typedef struct {
	uint32_t magic;
	uint8_t  freq;
	uint8_t  auto_prog_run_mode;
	uint8_t  auto_reset_after_pgm;
	uint8_t  append_endless_loop;
	uint8_t  speed_hotkey_mode;
	uint8_t  reserved1;
	uint8_t  reserved2;
	uint8_t  reserved3;
	uint32_t auto_prog_min_bytes;
	uint32_t checksum;
} PersistentSettings;

static uint32_t calc_settings_checksum(const PersistentSettings *s){
	return s->magic + (uint32_t)s->freq + (uint32_t)s->auto_prog_run_mode +
	       (uint32_t)s->auto_reset_after_pgm + (uint32_t)s->append_endless_loop +
	       (uint32_t)s->speed_hotkey_mode + s->auto_prog_min_bytes;
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
	HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
	LED_GPIO_Port->BRR = LED_Pin;
	CDC_Printf("\r\n\x1b[0m[Config Saved] Resuming Brainfuino (Clock: %s)...\r\n", get_freq_name(freq));
	initROMNormal();
	wait(1000);
	HAL_GPIO_WritePin(OE_GPIO_Port, OE_Pin, GPIO_PIN_RESET);
	wait(1000);
	// Pulse FPGA reset to restart execution cleanly with any new settings
	HAL_GPIO_WritePin(BF_RST_GPIO_Port, BF_RST_Pin, GPIO_PIN_RESET);
	HAL_Delay(10);
	HAL_GPIO_WritePin(BF_RST_GPIO_Port, BF_RST_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
	LED_GPIO_Port->BRR = LED_Pin;
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

uint8_t CDC_Receive_Callback(uint8_t *buff, uint32_t len){
	// Automated USB DFU trigger command (from build script or terminal)
	if (len >= 5 && strncmp((char *)buff, "!DFU!", 5) == 0){
		dfu_requested = 1;
		return 1;
	}

	if (state == STATE_RUN){
		if (is_prefix_ci(buff, len, "!MENU") || is_prefix_ci(buff, len, "!CONFIG")){
			menu_enter();
			return 1;
		}

		if (cfg_auto_prog_run_mode && (len >= cfg_auto_prog_min_bytes)){
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

		// Pass characters cleanly to the running FPGA soft-processor without interception
		if (len >= 1){
			writeBFInput(*buff);
			HAL_GPIO_WritePin(BF_INCMG_GPIO_Port, BF_INCMG_Pin, GPIO_PIN_SET);
			serial_input = 1;
		}
		return 1;
	}

	if (state == STATE_CONFIG){
		static uint32_t last_num_key_tick = 0;
		if (len == 0) return 1;

		// Standalone Spacebar: execute/toggle selected item
		if (len == 1 && buff[0] == ' '){
			menu_action_pending = menu_cursor;
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
					if (menu_cursor < 0) menu_cursor = MENU_ITEM_COUNT - 1;
					menu_needs_render = 1;
					return 1;
				}
				else if (buff[idx+2] == 'B'){ // Down Arrow
					menu_cursor++;
					if (menu_cursor >= MENU_ITEM_COUNT) menu_cursor = 0;
					menu_needs_render = 1;
					return 1;
				}
			}
			// Standalone ESC: exit menu
			menu_exit_requested = 1;
			return 1;
		}

		// 2. Direct numbered shortcuts (1-8, 0) and quick exit ('q' / 'Q')
		if (buff[idx] >= '1' && buff[idx] <= '8'){
			last_num_key_tick = HAL_GetTick();
			menu_cursor = buff[idx] - '1';
			menu_action_pending = menu_cursor;
			return 1;
		}
		if (buff[idx] == '0' || buff[idx] == 'q' || buff[idx] == 'Q'){
			menu_exit_requested = 1;
			return 1;
		}

		// 3. Enter / Return or Space: execute selected cursor item
		if (buff[idx] == '\r' || buff[idx] == '\n'){
			if (HAL_GetTick() - last_num_key_tick > 100){
				menu_action_pending = menu_cursor;
			}
			return 1;
		}
		if (buff[idx] == ' '){
			menu_action_pending = menu_cursor;
			return 1;
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

  // Load persistent settings from internal Flash
  settings_load();
  set_freq(freq);

  uint8_t init_btn = HAL_GPIO_ReadPin(BRD_RST_GPIO_Port, BRD_RST_Pin);
  btn_debounced = init_btn;
  btn_last_raw = init_btn;
  btn_raw_change_tick = HAL_GetTick();
  btn_press_tick = 0;
  btn_held_3s = 0;
  btn_held_6s = 0;
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

	  // 1. Debounced button state machine (35ms stable window to filter mechanical chatter)
	  uint8_t btn_raw = HAL_GPIO_ReadPin(BRD_RST_GPIO_Port, BRD_RST_Pin);

	  if (btn_raw != btn_last_raw){
		  btn_last_raw = btn_raw;
		  btn_raw_change_tick = HAL_GetTick();
	  }

	  if ((HAL_GetTick() - btn_raw_change_tick) >= 35){
		  if (btn_debounced != btn_last_raw){
			  btn_debounced = btn_last_raw;
			  if (btn_debounced == GPIO_PIN_RESET){
				  // Button officially PRESSED (held down)
				  btn_press_tick = HAL_GetTick();
				  btn_held_3s = 0;
				  btn_held_6s = 0;
				  btn_held_10s = 0;
				  // Hold FPGA in reset while physical button is held down
				  HAL_GPIO_WritePin(BF_RST_GPIO_Port, BF_RST_Pin, GPIO_PIN_RESET);
			  }
			  else {
				  // Button officially RELEASED
				  uint32_t press_duration = HAL_GetTick() - btn_press_tick;

				  if (btn_held_10s || (press_duration >= 10000)){
					  // Held >= 10s: Restore Default Brainfuino ASCII Logo Demo!
					  flashDefaultLogoProgram();
				  }
				  else if (btn_held_6s || (press_duration >= 6000)){
					  // Held >= 6s: Enter Interactive Configuration Menu
					  menu_enter();
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
				  else if (press_duration >= 20){
					  // Short press (< 3s)
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
						  CDC_Print("\r\n[Reset]\r\n");
					  }
				  }
				  btn_held_3s = 0;
				  btn_held_6s = 0;
				  btn_held_10s = 0;
				  if (state == STATE_RUN){
					  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
				  }
			  }
		  }
	  }

	  if (btn_debounced == GPIO_PIN_RESET){
		  // Button is currently being held down
		  uint32_t hold_time = HAL_GetTick() - btn_press_tick;
		  if (hold_time >= 10000){
			  btn_held_10s = 1;
			  // Rapid Red LED strobe (toggle every 50ms) at 10s mark
			  if ((hold_time / 50) % 2){
				  LED_GPIO_Port->BSRR = LED_Pin;
			  } else {
				  LED_GPIO_Port->BRR = LED_Pin;
			  }
		  }
		  else if (hold_time >= 6000){
			  btn_held_6s = 1;
			  // Visual indication: smooth pulse (period 600ms) at 6s mark for Config Menu threshold
			  led_update_pulse(600);
		  }
		  else if (hold_time >= 3000){
			  btn_held_3s = 1;
			  // Visual indication: Red LED turns ON solid at 3.0s (Program Mode threshold)
			  LED_GPIO_Port->BSRR = LED_Pin;
		  }
		  else {
			  // 0 to 3s hold: in config mode, keep solid ON for immediate tactile feedback
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
		  else if (menu_action_pending >= 0){
			  uint8_t act = (uint8_t)menu_action_pending;
			  menu_action_pending = -1;
			  menu_execute_action(act);
			  if (state == STATE_CONFIG){
				  menu_render();
			  }
		  }
		  else if (menu_needs_render){
			  menu_needs_render = 0;
			  menu_render();
		  }

		  else if (btn_debounced == GPIO_PIN_SET){
			  // Continuous smooth LED breathing/pulsing while in Config Menu (only when actively in menu)
			  led_update_pulse(1200);
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
			  // Program <= 4 kB: paste complete! Flash via unified engine
			  flashBufferToROM(prog_buffer, prog_rx_count, NULL, cfg_append_endless_loop, cfg_auto_reset_after_pgm);

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

	  // Check if throttled FPGA clock can resume after buffer drained
	  if (mco_throttled){
		  uint16_t used = (tail >= head) ? (tail - head) : (OUTBOX_CAPACITY - (head - tail));
		  if (used < (OUTBOX_CAPACITY / 2)){
			  mco_throttled = 0;
			  RCC->CFGR = (RCC->CFGR & ~(RCC_CFGR_MCO | RCC_CFGR_MCOPRE)) | active_mco_cfg;
		  }
	  }

	  // Send data out to host computer
	  if((state == STATE_RUN) && (head != tail) && !TxBusy()){
		  uint32_t cur_tail = tail;
		  leaving = head;
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

		  if (mco_throttled){
			  uint16_t used = (tail >= head) ? (tail - head) : (OUTBOX_CAPACITY - (head - tail));
			  if (used < (OUTBOX_CAPACITY / 2)){
				  mco_throttled = 0;
				  RCC->CFGR = (RCC->CFGR & ~(RCC_CFGR_MCO | RCC_CFGR_MCOPRE)) | active_mco_cfg;
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
