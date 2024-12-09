#pragma once
//#include <OneWireSlave.h>
#include <EEPROM.h>
#include <OLED_I2C.h>
//#include "GyverEncoder.h"
#include "TimerOne.h"

#include "DualFunctionButton.h"	
#include "defines.h"
//#include "cyfral.h"
#include "dallas.h"
#include "em_marine.h"
//OLED _OLED(SDA, SCL);
const boolean _use_hw = true;
OLED _OLED;
extern uint8_t SmallFont[];
extern uint8_t BigNumbers[];
//uint32_t stTimer;

DualFunctionButton BtnOK(BtnOKPin, 2000, INPUT_PULLUP);
DualFunctionButton BtnErase(BtnOKPin, 5000, INPUT_PULLUP);
DualFunctionButton BtnUp(BtnUpPin, 2000, INPUT_PULLUP);
DualFunctionButton BtnDown(BtnDownPin, 2000, INPUT_PULLUP);

//Encoder enc1(CLK, DT, BtnPin);

//OneWireSlave iBtnEmul(iBtnEmulPin);  //Эмулятор iButton для BlueMode
const byte MAX_KEYS = EEPROM.length() / 8 - 1;                   // максимальное кол-во ключей, которое влазит в EEPROM, но не > 20
byte EEPROM_key_count;               // количество ключей 0..MAX_KEYS, хранящихся в EEPROM
byte EEPROM_key_index = 0;           // 1..EEPROM_key_count номер последнего записанного в EEPROM ключа
byte addr[8];                        // временный буфер
byte keyID[8];                       // ID ключа для записи
//byte halfT;                          // полупериод для метаком
byte rom[8]{ 0x1, 0xBE, 0x40, 0x11, 0x5A, 0x36, 0x0, 0xE1 };

byte indxKeyInROM(const byte(&buf)[8]) {  //возвращает индекс или ноль если нет в ROM
	uint16_t idx = 0;
	for (byte count = 1, i = 0; count <= EEPROM_key_count; count++, i = 0, idx += 8) {  // ищем ключ в eeprom.
		do {
			if (EEPROM[idx + i] != buf[i]) break;
		} while (++i < 8);
		if (i == 8) return count;
	}
	return 0;
}

key_type getKeyType(const byte(&buf)[8]) {
	if (buf[0] == 0x01) return keyDallas;  // это ключ формата dallas
	switch (buf[0] >> 4) {
	case 1: return keyCyfral;
	case 2: return keyMetacom;
	case 0xF: if (vertParityCheck(buf)) return keyEM_Marine;
	}
}

void OLED_printKey(const byte(&buf)[8], bool keyIndex = false) {
	String str;
	if (!keyIndex) {
		str += "The key "; str += EEPROM_key_index; str += " of "; str += EEPROM_key_count; str += " in ROM";
	} else {
		auto index = indxKeyInROM(buf);
		if (index != 0) { str += "The key exists in ROM: "; str += index; } else { str += "Hold the Btn to save"; }
	}
	_OLED.clrScr(); _OLED.print(str, 0, 0);
	str = "";
	for (byte i = 0;;) { str += String(buf[i], HEX); if (++i < 8)str += ':'; else break; }  // это ключ формата dallas
	//keyType = keyDallas;
		if ((buf[0] == 1) && ibutton.crc8(buf, 7) != buf[7]) {
			str += "\n !CRC";
		}
	DEBUGLN(str);
	_OLED.print(str, 0, 12);
	str = "Type ";
	switch (keyType) {
	case keyDallas: str += "Dallas"; break;
	case keyCyfral: str += "Cyfral"; break;
	case keyMetacom: str += "Metakom"; break;
	case keyEM_Marine: str += "EM_Marine"; break;
	case keyUnknown: str += "Unknown"; break;
	}
	DEBUGLN(str);
	_OLED.print(str, 0, 24);
	_OLED.update();
}

void OLEDprint_error(byte err = 0) {
	//digitalWrite(R_Led, LOW);
	String str;
	_OLED.clrScr(); //,
	switch (err) {
	case NOERROR: str = F("Success"); break;
	case KEY_SAVED: str = F("Key saved"); break;
	default: 
	{ str = F("ERROR_"); 
		switch (err) {
			case ERROR_COPY: str += F("COPY");  break;
			case ERROR_UNKNOWN_KEY: str += F("UNKNOWN"); str += F("_KEY"); break;
			case ERROR_SAME_KEY: str += F("SAME"); str += F("_KEY"); break;
			case ERROR_RFID_TIMEOUT: str += F("RFID_TIMEOUT"); break;
			case ERROR_RFID_READ_TIMEOT: str += F("RFID_READ_TIMEOT"); break;
			case ERROR_RFID_PARITY: str += F("RFID_PARITY"); break;
		}
		str += "\t= "; str += err;
	}
	}
	DEBUGLN(str);
	_OLED.print(str, 0, 12);
	_OLED.update();
	//if (err) {Sd_ErrorBeep();}
	//else { Sd_ReadOK(); }
	//digitalWrite(R_Led, HIGH);
}

bool EPPROM_AddKey(const byte(&buf)[8]) {
	auto indx = indxKeyInROM(buf);  // ищем ключ в eeprom. Если находим, то не делаем запись, а индекс переводим в него
	if (indx != 0) {
		EEPROM_key_index = indx;
		EEPROM.update(EEPROM_KEY_INDEX, EEPROM_key_index);
		return false;
	}DEBUGLN(F("Adding to EEPROM\t"));
	for (byte i = 0;;) { DEBUGHEX(buf[i]); if (++i < 8)DEBUG(':'); else break; }DEBUGLN();
	if (EEPROM_key_count < MAX_KEYS) EEPROM_key_index = ++EEPROM_key_count;
	else EEPROM_key_index++; //EEPROM_key_count == MAX_KEYS
	if (EEPROM_key_index > EEPROM_key_count) EEPROM_key_index = 1;
	EEPROM.put((EEPROM_key_index - 1) * 8, buf);
	EEPROM.update(EEPROM_KEY_COUNT, EEPROM_key_count);
	EEPROM.update(EEPROM_KEY_INDEX, EEPROM_key_index);
	return true;
}

void EEPROM_get_key(byte(&buf)[8]) {
	if (EEPROM_key_index > MAX_KEYS) return;
	EEPROM.get((EEPROM_key_index - 1) * 8, buf);
	keyType = getKeyType(buf);
}


/*void clearLed() {
	digitalWrite(R_Led, LOW);
	digitalWrite(G_Led, LOW);
	digitalWrite(B_Led, LOW);
}*/
