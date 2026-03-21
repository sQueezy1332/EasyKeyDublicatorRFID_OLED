#pragma once
#include <stdint.h>
#include <stddef.h>
#define DEBUG_ENABLE
#ifdef DEBUG_ENABLE
#define DEBUG(x, ...) Serial.print(x, ##__VA_ARGS__)
#define DEBUGLN(x, ...) Serial.println(x, ##__VA_ARGS__)
#else
#define DEBUG(x, ...)
#define DEBUGLN(x, ...)
#endif

//settings
#define SIZE 8
#define rfidUsePWD 0    // ключ использует пароль для изменения
#define rfidPWD 123456  // пароль для ключа
#define RFID_BIT_PERIOD 64   // Скорость обмена с rfid 2 kbps
#define RFID_HALFBIT (1000 / (125 / (RFID_BIT_PERIOD / 2.0)))
//#define COMP_REG (ACSR & _BV(ACO))ss
#define COMP_PIN (2)
#ifdef __LGT8F__
#include "fastio_digital.h"
//#define COMP_REG (!fastioRead(D2))
//#define COMP_REG (!fastioRead(D2))
#define COMP_REG (C0SR & _BV(C0O))
#else
#define COMP_REG (!digitalRead(COMP_PIN))
#endif // __LGT8F__
#if F_CPU >= 32000000
#define DELAY_COMP (int)(RFID_HALFBIT / 8)	//TODO
#else
#define DELAY_COMP (int)(RFID_HALFBIT / 16)
#endif

/*Toggle on Compare Match on COM2A (pin 11) count timer2 until OCR2A //WGM2 mode 7 */
#define TIMER2MASK (_BV(COM2A0) | _BV(WGM21) | _BV(WGM20)) 
#define uS micros()
#define mS millis()
//pins
#define iButtonPin PIN_A3   // Линия data ibutton	
#define Luse_Led 13     // Светодиод лузы
#define R_Led 2         // RGB Led
#define G_Led 3
#define B_Led 4
#define ACpin 6        // Вход Ain0 аналогового компаратора 0.1В для EM-Marie
#define FreqGen 11     // генератор 125 кГц //PB3
#define speakerPin 12  // Спикер, он же buzzer, он же beeper
//#define CLK 8          // s1 энкодера
//#define DT 9           // s2 энкодера
#define BtnUpPin 8    // Кнопка вверх
#define BtnDownPin 9  // Кнопка вниз
#define BtnOKPin 10      // Кнопка переключения режима чтение/запись
#define EEPROM_KEY_COUNT (E2END)
#define EEPROM_KEY_INDEX (E2END - 1)
typedef uint8_t byte;

enum key_type {
	keyUnknown,
	keyDallas,
	keyCyfral,
	keyMetacom,
	keyEM_Marine,
};  // тип оригинального ключа

enum myMode : uint8_t {
	md_empty,
	md_read,
	md_write,
	md_blueMode
};  // режим работы копировальщика

enum emRWType {
	ERROR_READ_4 = -4,
	ERROR_READ_3 = -3,
	ERROR_READ_2 = -2,
	UNKNOWN_TYPE = -1,
	TM01 = 1001,
	RW1990_1 = 1991,
	RW1990_2 = 1992,
	TM2004 = 2004,
	EM4305 = 4305,
	T5557 = 5577,
};  // тип болванки

enum error_t {
	NOERROR = 0,
	KEY_SAVED,
	KEY_SAME,
	KEY_MISMATCH,
	ERROR_READ,
	ERROR_COPY,
	//ERROR_UNKNOWN_RFID_TYPE,
	ERROR_RFID_TIMEOUT = 'A', //A
	ERROR_RFID_COMP_TIMEOUT, //B
	ERROR_RFID_HEADER, //C
	ERROR_RFID_PARITY_ROW, //D
	ERROR_RFID_PARITY_COL, //E
	ERROR_RFID_STOP_BIT, //F
};
