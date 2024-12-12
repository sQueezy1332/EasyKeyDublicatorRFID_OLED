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
const byte MAX_KEYS = EEPROM.length() / 8 - 1;
byte EEPROM_key_count;               // количество ключей 0..MAX_KEYS, хранящихся в EEPROM
byte EEPROM_key_index = 0;           // 1..EEPROM_key_count номер последнего записанного в EEPROM ключа
byte buffer[8];                        // временный буфер
byte keyID[8];                       // ID ключа для записи
//byte halfT;                          // полупериод для метаком
byte rom[8]{ 0x1, 0xBE, 0x40, 0x11, 0x5A, 0x36, 0x0, 0xE1 };

byte indxKeyInROM(const byte(&buf)[8]) {  //возвращает индекс или ноль если нет в ROM
	uint16_t idx = 0;
	for (byte count = 1, i; count <= EEPROM_key_count; count++, idx += 8) {  // ищем ключ в eeprom.
		for (i = 0;;) {
			if (EEPROM[idx + i] != buf[i]) break;
			if (++i == 8) return count;
		}
	}
	return 0;
}

key_type getKeyType(const byte(&buf)[8]) {
	if (buf[0] == 0x1) return keyDallas;  // это ключ формата dallas
	switch (buf[0] >> 4) {
	case 0x1: return keyCyfral;
	case 0x2: return keyMetacom;
	case 0xF: return keyEM_Marine;
	}
	return keyUnknown;
}

void OLED_printKey(const byte(&buf)[8], bool keyIndex = false) {
	String str;
	if (!keyIndex) {
		str = "The key "; str += EEPROM_key_index; str += " of "; str += EEPROM_key_count; ;
	} else {
		auto index = indxKeyInROM(buf);
		if (index != 0) {
			str = "The key "; str += "exists: "; str += index; str += " of "; str += EEPROM_key_count;
		}
		else { str = "Hold Btn to save in "; 
		index = EEPROM_key_index + 1 > MAX_KEYS ? 1 : EEPROM_key_index + 1;
		str += index;  str += " index";
		}
	}
		_OLED.clrScr(); _OLED.print(str, 0, 0); DEBUGLN(str);
	str = "";
	for (byte temp, i = (keyType == keyEM_Marine) ? 5 : 7;; --i) {
		if(i == 0 && keyType == keyEM_Marine) break;
		temp = buf[i]; 
		if ((temp & 0xF0) == 0) str += '0';
		str += String(temp, HEX);
		if (i != 0) str += ':'; else break;
	}
	if ((keyType == keyDallas) && (ibutton.crc8(buf, 7) != buf[7])) { str += "\t !CRC"; }
		_OLED.print(str, 0, 12); DEBUGLN(str);
	str = "Type ";
	switch (keyType) {
	case keyDallas: str += "Dallas"; break;
	case keyCyfral: str += "Cyfral"; break;
	case keyMetacom: str += "Metakom"; break;
	case keyEM_Marine: str += "EM_Marine"; break;
	case keyUnknown: str += "Unknown"; break;
	}
		_OLED.print(str, 0, 24); DEBUGLN(str);
		_OLED.update();
}

void OLEDprint_error(byte err = 0) {
	//digitalWrite(R_Led, LOW);
	String str;
	//_OLED.clrScr(); //,'
	if(err == NOERROR)str = F("Success");
	else {
		str = F("ERROR_");
		switch (err) {
		case ERROR_COPY: str += F("COPY");  break;
		case ERROR_UNKNOWN_KEY: str += F("UNKNOWN"); str += F("_KEY"); break;
		case ERROR_SAME_KEY: str += F("SAME"); str += F("_KEY"); break;
		case ERROR_RFID_TIMEOUT: str += F("RFID_TIMEOUT"); break;
		case ERROR_RFID_READ_TIMEOT: str += F("RFID_READ_TIMEOT"); break;
		case ERROR_RFID_PARITY: str += F("RFID_PARITY"); break;
		}
		str += "\t# "; str += err;
	}
	DEBUGLN(str);
	_OLED.print(str, 0, 24);
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
