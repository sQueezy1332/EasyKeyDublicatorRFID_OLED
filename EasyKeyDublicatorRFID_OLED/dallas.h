#pragma once
#include <OneWire.h>
OneWire ibutton;
const uint8_t pin_onewire = iButtonPin;

emRWType getRWtype() {	
	// TM01 это неизвестный тип болванки, делается попытка записи TM-01 без финализации для dallas или c финализацией под cyfral или metacom
	// RW1990_1 - dallas-совместимые RW-1990, RW-1990.1, ТМ-08, ТМ-08v2
	// RW1990_2 - dallas-совместимая RW-1990.2
	// TM2004 - dallas-совместимая TM2004 в доп. памятью 1кб
	// пробуем определить RW-1990.1
	byte answer;
	ibutton.reset();
	ibutton.write(0xD1);   // пробуем снять флаг записи для RW-1990.1
	ibutton.write_bit(1);  // записываем значение флага записи = 1 - отключаем запись
	delay(10);
	pinMode(iButtonPin, INPUT);
	ibutton.reset();
	ibutton.write(0xB5);  // send 0xB5 - запрос на чтение флага записи
	answer = ibutton.read();
	DEBUGLN(F("Type: dallas "));
	//DEBUG(F("\n Answer RW-1990.1: ")); DEBUGLN(answer, HEX);
	if (answer == 0xFE) {
		DEBUGLN(F("RW-1990")); DEBUGLN(F(".1"));
		return RW1990_1;  // это RW-1990.1
	}
	// пробуем определить RW-1990.2
	ibutton.reset();
	ibutton.write(0x1D);   // пробуем установить флаг записи для RW-1990.2
	ibutton.write_bit(1);  // записываем значение флага записи = 1 - включаем запись
	delay(10);
	pinMode(iButtonPin, INPUT);
	ibutton.reset();
	ibutton.write(0x1E);  // send 0x1E - запрос на чтение флага записи
	answer = ibutton.read();
	if (answer == 0xFE) {
		ibutton.reset();
		ibutton.write(0x1D);   // возвращаем обратно запрет записи для RW-1990.2
		ibutton.write_bit(0);  // записываем значение флага записи = 0 - выключаем запись
		delay(10);
		pinMode(iButtonPin, INPUT);
		DEBUGLN(F("RW-1990")); DEBUGLN(F(".2"));
		return RW1990_2;  // это RW-1990.2
	}
	// пробуем определить TM-2004
	ibutton.reset();
	ibutton.write(0x33);                          // посылаем команду чтения ROM для перевода в расширенный 3-х байтовый режим
	for (byte i = 0; i < 8; i++) ibutton.read();  // читаем данные ключа
	ibutton.write(0xAA);                          // пробуем прочитать регистр статуса для TM-2004
	ibutton.write(0x00);
	ibutton.write(0x00);					// передаем адрес для считывания       
	if (0x9C == ibutton.read()) {			// читаем CRC комманды и адреса
		answer = ibutton.read();			// читаем регистр статуса
		DEBUGLN(F("TM2004")); DEBUG(F(" status: ")); DEBUGHEX(answer);
		ibutton.reset();
		return TM2004;  // это Type: TM2004
	}
	ibutton.reset();
	DEBUGLN(F("unknown, trying TM-01!"));
	return TM01;  // это неизвестный тип DS1990, нужно перебирать алгоритмы записи (TM-01)
}

bool writeTM2004(const byte(&data)[8]) {  // функция записи на TM2004
	ibutton.reset();
	ibutton.write(0x3C);  // команда записи ROM для TM-2004
	ibutton.write(0x00);
	ibutton.write(0x00);  // передаем адрес с которого начинается запись
	const byte led_state = digitalRead(R_Led);
	byte i = 0, red = led_state;
	do {
		digitalWrite(R_Led, red = !red);
		ibutton.write(data[i]);
		//if (0x65 != ibutton.read()) { return false; }     // crc not correct
		delayMicroseconds(600);
		ibutton.write_bit(1);
		delay(50);  
		pinMode(iButtonPin, INPUT);
		DEBUG('*');//Sd_WriteStep();
		if (data[i] != ibutton.read()) { 
			break;
		}
	}while (++i < 8);
	ibutton.reset();
	digitalWrite(R_Led, led_state);
	return i < 8 ? false : true;
}

void convert_MC(byte(&buf)[8]) {
	byte i, BYTE, bitmask, data;
	for (i = 0; i < 5; i++) {
		data = ~buf[i];
		for (BYTE = 0, bitmask = 128; bitmask; bitmask >>= 1, data >>= 1) {
			if (data & 1) BYTE |= bitmask;
		}
		buf[i] = BYTE;
	}
	buf[4] &= 0xF0;
	do { buf[i] = 0; } while (++i < 8); //memset(&buf[i], 0, 3);
}

bool dataBurningOK(byte bitCnt, const byte(&data)[8], byte(&buf)[8]) {
	if (!ibutton.reset()) return false;
	ibutton.write(0x33);
	ibutton.read_bytes(buf, 8);
	if (bitCnt == 36) convert_MC(buf);
	for (byte i = 0;;) {
		DEBUGHEX(buf[i]);
		if (data[i] != buf[i]) return false;  //compare data
		if (++i < 8)DEBUG(':'); else break;
	}
	return true;
}

void BurnByte(byte data) {
	for (byte bitmask = 0; bitmask; bitmask >>= 1) {
		ibutton.write_bit(data & bitmask);
		delay(5);          // time to writing for every bit up to 10 ms
	}
	pinMode(iButtonPin, INPUT);
}

void BurnByteMC(const byte(&data)[8]) {
	for (byte n_bit = 0, bitmask = 128; n_bit < 36; n_bit++) {
		ibutton.write_bit(!((data[n_bit >> 3]) & bitmask));
		delay(5);  // даем время на прошивку каждого бита 5 мс
		if ((bitmask >>= 1) == 0) bitmask = 128;
	}
	pinMode(iButtonPin, INPUT);
}

bool writeRW1990_1_2_TM01(const byte(&data)[8], byte(&buf)[8], emRWType rwType) {  // RW1990.1, RW1990.2, TM-01C(F)
	byte rwCmd, bitCnt = 64, rwFlag = 1;
	switch (rwType) {
	case TM01:
		rwCmd = 0xC1;
		if (keyType > keyDallas) bitCnt = 36;
		break;  //TM-01C(F)
	case RW1990_1:
		rwCmd = 0xD1;
		rwFlag = 0;	// RW1990.1   writing flag is enverted
		break;                             
	case RW1990_2: 
		rwCmd = 0x1D; 
		break;  // RW1990.2
	}
	ibutton.reset();
	ibutton.write(rwCmd);       // send 0xD1 - writing command
	ibutton.write_bit(rwFlag);  // writing flag  1 - enable writing
	delay(5);
	ibutton.reset();
	if (rwType == TM01) ibutton.write(0xC5);
	else ibutton.write(0xD5);  // команда на запись
	if (bitCnt != 36) {
		const byte iMax = (bitCnt >> 3), led_state = digitalRead(R_Led);
		for (byte i = 0, flag = led_state; i < iMax; i++) {
			digitalWrite(R_Led, flag = !flag);
			if (rwType == RW1990_1) BurnByte(data[i] ^ 0xFF);  // for RW1990.1 the writing is inverse
			else BurnByte(data[i]);
			DEBUG('*');
			//Sd_WriteStep();
		}
		ibutton.write(rwCmd);        // send 0xD1 - writing command
		ibutton.write_bit(!rwFlag);  // writing flag  1 - disable writing
		delay(5);
		digitalWrite(R_Led, led_state);
	}
	else BurnByteMC(data);
	if (dataBurningOK(bitCnt, data, buf) == false) { 
		return false; 
	}
	if (keyType > keyDallas) {  //finalisation	//translate key from dallas format
		ibutton.reset();
		if (keyType == keyCyfral) ibutton.write(0xCA);  
		else ibutton.write(0xCB);                       
		ibutton.write_bit(1); //finalisation flag                        
	}
	return true;
}

bool read_dallas(byte (&buf)[8]) {
	if (!ibutton.reset()) { return false; }
	ibutton.read_bytes(buf, 8);
	return true;
}

byte write_ibutton(byte(&data)[8]) { 
	byte buf[8];
	if (!read_dallas(buf)) { return false; }
	//DEBUG(F("The new key code is: "));
	for (byte i = 0;;) {
		DEBUGHEX(buf[i]);
		if (data[i] != buf[i]) break; // сompare keys
		if (++i < 8) DEBUG(':');
		else return ERROR_SAME_KEY;// writing not needed
	}
	auto type = getRWtype();  // type definition: RW-1990.1 or 1990.2 or TM-01
	DEBUG(F("\n Burning iButton ID: "));
	if (type == TM2004) {
		if (!writeTM2004(data)) {//шьем TM2004
			return ERROR_COPY;
		} 
	} else {
		if (!writeRW1990_1_2_TM01(data, buf, type)) {//try another format
			return ERROR_COPY;
		}
	}
	return NOERROR;
}

void SendDallas(byte* buf) {
	/*  iBtnEmul.init(buf);
	//iBtnEmul.waitForRequest(false);
	uint32_t tStart = millis();
	do {
	  if (!iBtnEmul.waitReset(10) ) continue;
	  if (!iBtnEmul.presence() ) continue;
	  if (iBtnEmul.recvAndProcessCmd() ) break;
	} while (millis() < 200 + tStart);  */
}
