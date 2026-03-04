#pragma once
//#include <OneWireSlave.h>
#include <EEPROM.h>
#include <OLED_I2C.h>
//const boolean _use_hw = true;
//#include "GyverEncoder.h"
#define USE_MICRO_WIRE
#include <microWire.h>
#include <GyverOLED.h>
#include "DualFunctionButton.h"	
#include "defines.h"
#include "dallas.h"
#include <CyfralMetakom.h>
#include "em_marine.h"

//OLED OLED(SDA, SCL);

GyverOLED <SSH1106_128x64, OLED_NO_BUFFER, OLED_I2C> OLED;
extern uint8_t SmallFont[];
extern uint8_t BigNumbers[];
//uint32_t stTimer;

DualFunctionButton BtnOK(BtnOKPin, 2000, INPUT_PULLUP);
DualFunctionButton BtnErase(BtnOKPin, 5000, INPUT_PULLUP);
DualFunctionButton BtnUp(BtnUpPin, 2000, INPUT_PULLUP);
DualFunctionButton BtnDown(BtnDownPin, 2000, INPUT_PULLUP);

extern void rfid_emul_high_impl() { pinMode(FreqGen, INPUT); };
extern void rfid_emul_low_impl() { pinMode(FreqGen, OUTPUT); };
//Encoder enc1(CLK, DT, BtnPin);

//OneWireSlave iBtnEmul(iBtnEmulPin);  //Эмулятор iButton для BlueMode
const byte MAX_KEYS = EEPROM.length() / 8 - 1;
byte EEPROM_key_count;               // количество ключей 0..MAX_KEYS, хранящихся в EEPROM
byte EEPROM_key_index = 0;           // 1..EEPROM_key_count номер последнего записанного в EEPROM ключа
byte Buffer[8];						// временный буфер
byte keyID[8];                       // ID ключа для записи

byte keyType;
myMode Mode = md_empty;
//byte halfT;                          // полупериод для метаком
//byte rom[8]{ 0x1, 0xBE, 0x40, 0x11, 0x5A, 0x36, 0x0, 0xE1 };

bool op_amp() {
	static auto prev_state = COMP_REG;
	const auto state = COMP_REG;
	if (state != prev_state) {
		for (auto time = uS; COMP_REG == state; ) {
			if (uS - time > DELAY_COMP) {
				return prev_state = state;
			}
		}
	}
	return prev_state;
}

void rfid_pwm_disable() {
	TCCR2A = 0; pinMode(FreqGen, INPUT); /*digitalWrite(FreqGen, LOW);*/// Оключить ШИМ COM2A(pin 11)
}

void rfid_pwm_enable() {
	pinMode(FreqGen, OUTPUT); TCCR2A = TIMER2MASK; //Toggle on Compare Match on COM2A (pin 11) и счет таймера2 до OCR2A
}

void rfid_init() { //включаем генератор 125кГц
	TCCR2B = _BV(WGM22) | _BV(CS20);		 // mode 7: Fast PWM //divider 1 (no prescaling)
	OCR2A = (F_CPU / 1 / 125000 / 2) - 1;
	// включаем компаратор
	ADCSRB &= ~_BV(ACME);  // отключаем мультиплексор AC
	ACSR &= ~_BV(ACBG);    // отключаем от входа Ain0 1.1V
	rfid_pwm_enable();
	delay(10);       //13 мс длятся переходные процессы детектора
}

byte indxKeyInROM(const byte(&buf)[8]) {  //возвращает индекс или ноль если нет в ROM
	size_t idx = 0;
	for (byte count = 1, i; count <= EEPROM_key_count; count++, idx += 8) {  // ищем ключ в eeprom.
		for (i = 0;;) {
			if (EEPROM[idx + i] != buf[i]) break;
			if (++i == 8) return count;
		}
	}
	return 0;
}

key_type getKeyType(const byte* buf) {
	switch (buf[0]) {
	case 0x1:return keyDallas;
	case 0x2: return keyCyfral;
	case 0x3: return keyMetacom;
	case 0xFF: return keyEM_Marine;
	}
	return keyUnknown;
}

template <bool big_endian, char separ> void bytes_to_str(char* ptr, const byte* buf, byte data_size) {
	if (data_size == 0) return;
	byte i, num, shift, nibble; char inc;
	if (big_endian) { i = 0; --data_size; inc = 1; }
	else { i = data_size - 1; data_size = 0; inc = -1; }
	for (;; i += inc) {
		for (shift = 4, num = buf[i];; shift = 0) {
			nibble = (num >> shift) & 0xF;
			*ptr++ = nibble < 10 ? nibble ^ 0x30 : nibble + ('A' - 10);
			if (shift == 0) break;
		}
		if (i == data_size) break;
		if (separ) { *ptr++ = separ; }
	}
	*ptr = '\0';
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
	//OLED.clrScr(); OLED.print(str, 0, 0); DEBUGLN(str);
		str.reserve(3 * 8); 
		bytes_to_str<true, ':'>(str.begin(), buf, keyType == keyEM_Marine ? 6 : 8);
			/*char* const c = str.begin();
			for (byte temp, i = (keyType == keyEM_Marine) ? 5 : 7;; --i, c+=3) {
				if (i == 0 && keyType == keyEM_Marine) break;
				
				
				temp = buf[i];
				if ((temp & 0xF0) == 0) *c = '0';
				utoa(temp, &c[(temp & 0xF0) ? 0 : 1], HEX);
				if (i == 0) break; 
				c[2] = ':';
			}*/
	byte ex_crc = ibutton.crc8(buf, 7);
	if (keyType == keyDallas && ex_crc != buf[7]) { str += F("\t !=CRC ");  str += String(ex_crc, HEX);  }
		//OLED.print(str, 0, 12); 
	DEBUGLN(str);
	str = F("Type ");
	switch (keyType) {
	case keyDallas: str += F("Dallas"); break;
	case keyCyfral: str += F("Cyfral"); break;
	case keyMetacom: str += F("Metakom"); break;
	case keyEM_Marine: str += F("EM_Marine"); break;
	case keyUnknown: str += F("UNKNOWN"); break;
	}
		//OLED.print(str, 0, 24); DEBUGLN(str);
		//OLED.update();
}

void OLEDprint_error(byte err = 0) {
	//digitalWrite(R_Led, LOW);
	String str;
	//OLED.clrScr(); //,'
	if(err == NOERROR)str = F("Success");
	else {
		str = F("ERROR_");
		switch (err) {
		case ERROR_COPY: str += F("COPY");  break;
		case KEY_SAME: str += F("SAME"); str += F("_KEY"); break;
		case ERROR_UNKNOWN_KEY: str += F("UNKNOWN"); str += F("_KEY"); break;
		case ERROR_RFID_TIMEOUT: str += F("RFID_TIMEOUT"); break;
		case ERROR_RFID_HEADER: str += F("RFID_HEADER"); break;
		case ERROR_RFID_PARITY_ROW: str += F("PARITY_ROW"); break;
		case ERROR_RFID_PARITY_COL: str += F("PARITY_COL"); break;
		}
		str += "\t# "; str += err;
	}
	DEBUGLN(str);
	//OLED.print(str, 0, 24);
	//OLED.update();
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
	for (byte i = 0;;) { DEBUG(buf[i], HEX); if (++i < 8)DEBUG(':'); else break; }DEBUGLN();
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

uint32_t pulseACompA(bool pulse, byte Average = 80, uint32_t timeOut = 1500) {  // pulse HIGH or LOW
	bool AcompState;
	uint32_t tEnd = micros() + timeOut;
	do {
		ADCSRA |= (1 << ADSC);
		while (ADCSRA & (1 << ADSC));  // Wait until the ADSC bit has been cleared
		if (ADCH > 200) return 0;
		if (ADCH > Average) AcompState = HIGH;  // читаем флаг компаратора
		else AcompState = LOW;
		if (AcompState == pulse) {
			tEnd = micros() + timeOut;
			do {
				ADCSRA |= (1 << ADSC);
				while (ADCSRA & (1 << ADSC));	// Wait until the ADSC bit has been cleared
				if (ADCH > Average) AcompState = HIGH;  // читаем флаг компаратора
				else AcompState = LOW;
				if (AcompState != pulse) return (uint32_t)(micros() + timeOut - tEnd);
			} while (micros() < tEnd);
			return 0;  //таймаут, импульс не вернуся оратно
		}            // end if
	} while (micros() < tEnd);
	return 0;
}
void ADCsetOn() {
	ADMUX = (ADMUX & 0b11110000) | 0b0011 | (1 << ADLAR);                // (1 << REFS0);          // подключаем к AC Линию A3 ,  левое выравние, измерение до Vcc
	ADCSRB = (ADCSRB & 0b11111000) | (1 << ACME);                        // источник перезапуска ADC FreeRun, включаем мультиплексор AC
	ADCSRA = (ADCSRA & 0b11111000) | 0b011 | (1 << ADEN) | (1 << ADSC);  // | (1<<ADATE);      // 0b011 делитель скорости ADC, // включаем ADC и запускаем ADC и autotriger ADC
}
void ACsetOn() {
	ACSR |= 1 << ACBG;                      // Подключаем ко входу Ain0 1.1V для Cyfral/Metacom
	ADCSRA &= ~(1 << ADEN);                 // выключаем ADC
	ADMUX = (ADMUX & 0b11110000) | 0b0011;  // подключаем к AC Линию A3
	ADCSRB |= 1 << ACME;                    // включаем мультиплексор AC
}



/*void clearLed() {
	digitalWrite(R_Led, LOW);
	digitalWrite(G_Led, LOW);
	digitalWrite(B_Led, LOW);
}*/

