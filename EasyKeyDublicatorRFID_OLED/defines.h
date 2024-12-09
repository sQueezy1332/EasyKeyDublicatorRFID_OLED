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


//keyID[0] = 0xFF; keyID[1] = 0xA9; keyID[2] =  0x8A; keyID[3] = 0xA4; keyID[4] = 0x87; keyID[5] = 0x78; keyID[6] = 0x98; keyID[7] = 0x6A;//это чей-то рабочий код RFID. тут можно
		  //подменять каждое число. копир, при прикладывании ключа, будет выдавать эти числа и их можно записать в память. для RFID надо вычислять код
		  //keyID[0] = 0xFF; keyID[1] = 0xFB; keyID[2] =  0xDE; keyID[3] = 0xF7; keyID[4] = 0xBD; keyID[5] = 0xEF; keyID[6] = 0x7B; keyID[7] = 0xC0;// RFID FF FF FF FF FF METAKOM_CYFRAL
		  //keyID[0] = 0xFF; keyID[1] = 0x80; keyID[2] =  0x7E; keyID[3] = 0xF7; keyID[4] = 0xBD; keyID[5] = 0xEF; keyID[6] = 0x7B; keyID[7] = 0xC2;// RFID 01 FF FF FF FF  CYFRAL
		  //keyID[0] = 0xFF; keyID[1] = 0xFB; keyID[2] =  0xDE; keyID[3] = 0xF7; keyID[4] = 0xBD; keyID[5] = 0xEF; keyID[6] = 0x00; keyID[7] = 0x62;// RFID FF FF FF FF 01  CYFRAL ПЕРЕВЕРТЫШ
		  //keyID[0] = 0xFF; keyID[1] = 0x99; keyID[2] =  0x8A; keyID[3] = 0xA0; keyID[4] = 0xC6; keyID[5] = 0x90; keyID[6] = 0x5F; keyID[7] = 0xB6;// RFID 36 5A 11 40 BE VIZIT old_code
		  //keyID[0] = 0xFF; keyID[1] = 0xDF; keyID[2] =  0xA9; keyID[3] = 0x00; keyID[4] = 0xC6; keyID[5] = 0xAA; keyID[6] = 0x19; keyID[7] = 0x96;// RFID BE 40 11 5A 36 VIZIT ПЕРЕВЕРТЫШ
		  //keyID[0] = 0xFF; keyID[1] = 0xA9; keyID[2] =  0x8A; keyID[3] = 0xA0; keyID[4] = 0xC6; keyID[5] = 0x90; keyID[6] = 0x5F; keyID[7] = 0xBA;// RFID 56 5A 11 40 BE VIZIT
		  //keyID[0] = 0xFF; keyID[1] = 0xDF; keyID[2] =  0xA9; keyID[3] = 0x00; keyID[4] = 0xC6; keyID[5] = 0xAA; keyID[6] = 0x29; keyID[7] = 0x9A;// RFID BE 40 11 5A 56 VIZIT ПЕРЕВЕРТЫШ
		  //keyID[0] = 0xFF; keyID[1] = 0x80; keyID[2] =  0x00; keyID[3] = 0x00; keyID[4] = 0x00; keyID[5] = 0x00; keyID[6] = 0x00; keyID[7] = 0x00;// RFID 00 00 00 00 00 ELtis
		  //keyID[0] = 0x01; keyID[1] = 0xBE; keyID[2] =  0x40; keyID[3] = 0x11; keyID[4] = 0x5A; keyID[5] = 0x36; keyID[6] = 0x00; keyID[7] = 0xE1;//01 BE 40 11 5A 36 00 E1 для Vizit
		  //keyID[0] = 0x01; keyID[1] = 0xBE; keyID[2] =  0x40; keyID[3] = 0x11; keyID[4] = 0x5A; keyID[5] = 0x56; keyID[6] = 0x00; keyID[7] = 0xBB;//01 BE 40 11 5A 56 00 BB для Vizit
		  //keyID[0] = 0x01; keyID[1] = 0xFF; keyID[2] =  0xFF; keyID[3] = 0xFF; keyID[4] = 0xFF; keyID[5] = 0xFF; keyID[6] = 0xFF; keyID[7] = 0x2F;//01 FF FF FF FF FF FF 2F метако,цифра,ВИЗИ
		  //keyID[0] = 0x01; keyID[1] = 0xFF; keyID[2] =  0xFF; keyID[3] = 0xFF; keyID[4] = 0xFF; keyID[5] = 0x00; keyID[6] = 0x00; keyID[7] = 0x9B;//01 FF FF FF FF 00 00 9B метаком,ВИЗИТ
		  //keyID[0] = 0x01; keyID[1] = 0xFF; keyID[2] =  0xFF; keyID[3] = 0x01; keyID[4] = 0x00; keyID[5] = 0x00; keyID[6] = 0x00; keyID[7] = 0x2D;//01 FF FF 01 00 00 00 2D цифрал
		  //keyID[0] = 0x01; keyID[1] = 0x00; keyID[2] =  0x00; keyID[3] = 0x00; keyID[4] = 0x00; keyID[5] = 0x00; keyID[6] = 0x00; keyID[7] = 0x3D;//01 00 00 00 00 00 00 3D цифрал +
		  //keyID[0] = 0x01; keyID[1] = 0xA9; keyID[2] =  0xE4; keyID[3] = 0x3C; keyID[4] = 0x09; keyID[5] = 0x00; keyID[6] = 0x00; keyID[7] = 0xE6;//01 A9 E4 3C 09 00 00 E6 Элтис до 90%
		  //memcpy(keyID, rom, 8);

