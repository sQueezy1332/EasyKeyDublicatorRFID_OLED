#pragma once
#define DEBUG_ENABLE
#ifdef DEBUG_ENABLE
#define DEBUG(x) Serial.print(x)
#define DEBUGHEX(x) Serial.print(x, HEX)
#define DEBUGLN(x) Serial.println(x)
#else
#define DEBUG(x)
#define DEBUGHEX(x)
#define DEBUGLN(x)
#endif
//settings
#define rfidUsePWD 0    // ключ использует пароль для изменения
#define rfidPWD 123456  // пароль для ключа
#define rfidBitRate 2   // Скорость обмена с rfid в kbps
#define COMPARATOR (ACSR & _BV(ACO))
#define TIMER2MASK (_BV(COM2A0) | _BV(COM2B1) | _BV(WGM21) | _BV(WGM20))
//pins
#define iButtonPin PIN_A3   // Линия data ibutton
//#define iBtnEmulPin A1  // Линия эмулятора ibutton
#define Luse_Led 13     // Светодиод лузы
#define R_Led 2         // RGB Led
#define G_Led 3
#define B_Led 4
#define ACpin 6        // Вход Ain0 аналогового компаратора 0.1В для EM-Marie
#define speakerPin 12  // Спикер, он же buzzer, он же beeper
#define FreqGen 11     // генератор 125 кГц
//#define CLK 8          // s1 энкодера
//#define DT 9           // s2 энкодера
#define BtnUpPin 8    // Кнопка вверх
#define BtnDownPin 9  // Кнопка вниз
#define BtnOKPin 10      // Кнопка переключения режима чтение/запись
#define EEPROM_KEY_COUNT (E2END)
#define EEPROM_KEY_INDEX (E2END - 1)

enum key_type : uint8_t {
	keyUnknown,
	keyDallas,
	keyCyfral,
	keyMetacom,
	keyEM_Marine
};  // тип оригинального ключа

enum myMode : uint8_t {
	md_empty,
	md_read,
	md_write,
	md_blueMode
};  // режим работы копировальщика

enum emRWType : uint8_t {
	Unknown,
	TM2004,
	RW1990_1,
	RW1990_2,
	TM01,
	T5557,
	EM4305
};  // тип болванки

key_type keyType;
myMode Mode = md_empty;