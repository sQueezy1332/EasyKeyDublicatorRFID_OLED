#pragma once
#include <OneWire.h>
OneWire <iButtonPin> ibutton;
extern byte Buffer[8];
extern byte keyType;

emRWType getRWtype() {
	// TM01 это неизвестный тип болванки, делается попытка записи TM-01 без финализации для dallas или c финализацией под cyfral или metacom
	// RW1990_1 - dallas-совместимые RW-1990, RW-1990.1, ТМ-08, ТМ-08v2
	// RW1990_2 - dallas-совместимая RW-1990.2
	// TM2004 - dallas-совместимая TM2004 в доп. памятью 1кб
	// пробуем определить RW-1990.1
	byte answer;
	if (!ibutton.reset()) return UNKNOWN_TYPE;
	ibutton.write(0xD1);   // пробуем снять флаг записи для RW-1990.1
	ibutton.write_bit(1);  // записываем значение флага записи = 1 - отключаем запись
	delay(10);
	//pinMode(iButtonPin, INPUT);
	if(!ibutton.reset()) return UNKNOWN_TYPE;
	ibutton.write(0xB5);  // send 0xB5 - запрос на чтение флага записи
	answer = ibutton.read();
	DEBUGLN(F("Type: dallas "));
	//DEBUG(F("\n Answer RW-1990.1: ")); DEBUGLN(answer, HEX);
	if (answer == 0xFE) {
		DEBUGLN(F("RW-1990")); DEBUGLN(F(".1"));
		return RW1990_1;  // это RW-1990.1
	}
	// пробуем определить RW-1990.2
	if (!ibutton.reset()) return UNKNOWN_TYPE;
	ibutton.write(0x1D);   // пробуем установить флаг записи для RW-1990.2
	ibutton.write_bit(1);  // записываем значение флага записи = 1 - включаем запись
	delay(10);
	//pinMode(iButtonPin, INPUT);
	if (!ibutton.reset()) return UNKNOWN_TYPE;
	ibutton.write(0x1E);  // send 0x1E - запрос на чтение флага записи
	answer = ibutton.read();
	if (answer == 0xFE) {
		if (!ibutton.reset()) return UNKNOWN_TYPE;
		ibutton.write(0x1D);   // возвращаем обратно запрет записи для RW-1990.2
		ibutton.write_bit(0);  // записываем значение флага записи = 0 - выключаем запись
		delay(10);
		//pinMode(iButtonPin, INPUT);
		DEBUGLN(F("RW-1990")); DEBUGLN(F(".2"));
		return RW1990_2;  // это RW-1990.2
	}
	// пробуем определить TM-2004
	if (!ibutton.reset()) return UNKNOWN_TYPE;
	ibutton.write(0x33);                          // посылаем команду чтения ROM для перевода в расширенный 3-х байтовый режим
	for (byte i = 0; i < 8; i++) ibutton.read();  // читаем данные ключа
	ibutton.write(0xAA);                          // пробуем прочитать регистр статуса для TM-2004
	ibutton.write(0x00);
	ibutton.write(0x00);					// передаем адрес для считывания       
	if (0x9C == ibutton.read()) {			// читаем CRC комманды и адреса
		answer = ibutton.read();			// читаем регистр статуса
		DEBUGLN(F("TM2004 ")); DEBUG(F("status: ")); DEBUG(answer, HEX);
		//ibutton.reset();
		return TM2004;  // это Type: TM2004
	}
	//ibutton.reset();
	DEBUGLN(F("Unknown, trying TM-01"));
	return TM01;  // это неизвестный тип DS1990, нужно перебирать алгоритмы записи (TM-01)
}

bool writeTM2004(const byte data [8]) {  // функция записи на TM2004
	ibutton.reset();
	ibutton.write(0x3C);  // команда записи ROM для TM-2004
	ibutton.write(0x00);
	ibutton.write(0x00);  // передаем адрес с которого начинается запись
	byte i = ibutton.read();
	if (0x65 != i) {
		DEBUGLN(F("TM2004 ")); DEBUG(F("crc: ")); DEBUG(i, HEX);
		return false;
	}
	//const byte led_state = digitalRead(R_Led);
	//byte red = led_state;
	i = 0;
	do {
		//digitalWrite(R_Led, red = !red);
		ibutton.write(data[i]);
		    // crc not correct
		delayMicroseconds(600);
		ibutton.write_bit(1);
		delay(50);
		pinMode(iButtonPin, INPUT);
		DEBUG('*');//Sd_WriteStep();
		if (data[i] != ibutton.read()) {
			break;
		}
	} while (++i < 8);
	ibutton.reset();
	//digitalWrite(R_Led, led_state);
	return i == 8;
}

void convert_MC(byte buf[8]) {
	byte i, BYTE, bitmask, data;
	//*(uint64_t*)&buf >>= 4;
	for (i = 0; i < 5; i++) {
		data = buf[i] ^ 0xFF;
		for (BYTE = 0, bitmask = 128; bitmask; bitmask >>= 1, data >>= 1) {
			if (data & 1) BYTE |= bitmask;
		}
		buf[i] = BYTE;
	}
	buf[4] &= 0xF0;
	do { buf[i] = 0; } while (++i < 8); //memset(&buf[i], 0, 3);
}

bool read_dallas(byte buf[8]) {
	auto time = mS;
	while (!ibutton.search(buf)) {
		if (mS - time > 10) { return false; }
	}
	return true;
}

bool checkBurningMC(const byte data[8]) {
	auto time = mS;
	while (!ibutton.reset()) { if (mS - time > 30) { return false; } }
	ibutton.write(0x33);
	for (byte i = 0, BYTE, bitmask;; ++i) {
		for (BYTE = 0, bitmask = 128; bitmask; bitmask >>= 1) {
			if (i == 0 && bitmask == _BV(3)) break;
			if (ibutton.read_bit() == 0) BYTE |= bitmask;
		}
		DEBUG(BYTE, HEX);
		if (data[i] != BYTE) return false;  //compare data
		if (i < 4) DEBUG(':'); else break;
	}
}

bool checkBurningDallas(const byte data[8]) {
	if (!read_dallas(Buffer)) return false;
	for (byte i = 0;; ++i) {
		DEBUG(Buffer[i], HEX);
		if (data[i] != Buffer[i]) return false;  //compare data
		if (i < 7)DEBUG(':'); else break;
	}
}

void BurnByteDallas(byte data) {
	for (byte bitmask = 1; bitmask; bitmask <<= 1) {
		ibutton.write_bit(data & bitmask);
		delay(5);          // time to writing for every bit up to 10 ms
	}
	//pinMode(iButtonPin, INPUT);
}

void BurnByteMC(const byte data[8]) {
	for (byte i = 0, bitmask; i < 5; i++) {
		for (bitmask = 128; bitmask; bitmask >>= 1) {
			if (i == 0 && bitmask == _BV(3)) break;
			ibutton.write_bit(!((data[i]) & bitmask));
			delay(5);  // даем время на прошивку каждого бита 5 мс
		}
	}
	//pinMode(iButtonPin, INPUT);
}

bool writeRW1990_1_2_TM01(const byte data[8], const emRWType rwType) {  // RW1990.1, RW1990.2, TM-01C(F)
	byte Cmd, Flag = 1;
	switch (rwType) {
	case TM01:	Cmd = 0xC1; //TM-01C(F)
		break;  
	case RW1990_1: Cmd = 0xD1; Flag = 0;  //writing flag is enverted
		break;
	case RW1990_2: Cmd = 0x1D; 
		break;  
	}
	if(!ibutton.reset()) return false;
	ibutton.write(Cmd);       // send 0xD1 - writing command
	ibutton.write_bit(Flag);  // writing flag  1 - enable writing
	delay(10);
	if (!ibutton.reset()) return false;
	if (rwType == TM01) ibutton.write(0xC5);
	else ibutton.write(0xD5);  // команда на запись
	if (keyType <= keyDallas) {
		//const byte led_state = digitalRead(R_Led);
		for (byte i = 0/*, flag = led_state*/; i < 8; i++) {
			//digitalWrite(R_Led, flag = !flag);
			if (rwType == RW1990_1) BurnByteDallas(data[i] ^ 0xFF);  // for RW1990.1 the writing is inverse
			else BurnByteDallas(data[i]);
			DEBUG('*');//Sd_WriteStep();
		}
		ibutton.write(Cmd);        // send 0xD1 - writing command
		ibutton.write_bit(!Flag);  // writing flag  1 - disable writing
		delay(10); 
		//digitalWrite(R_Led, led_state);
		if (!checkBurningDallas(data)) return false;
	} else {
		BurnByteMC(data);
		if (!checkBurningMC(data)) return false;
	}
	ibutton.reset();
	switch (keyType) {
	case keyCyfral: ibutton.write(0xCA);
	case keyMetacom: ibutton.write(0xCB);
	default: return true;
	}
	ibutton.write_bit(1); //finalisation flag 
	return true;
	
}

byte write_ibutton(byte(&data)[8]) {
	if (!read_dallas(Buffer)) { return false; }
	//DEBUG(F("The new key code is: "));
	for (byte i = 0;;) {
		DEBUG(Buffer[i], HEX);
		if (data[i] != Buffer[i]) break; // сompare keys
		if (++i < 8) DEBUG(':');
		else return KEY_SAME;// writing not needed
	}
	auto type = getRWtype();  // type definition: RW-1990.1 or 1990.2 or TM-01
	DEBUG(F("\n Burning iButton ID: "));
	switch (type) {
	case TM2004: if (!writeTM2004(data)) return ERROR_COPY;
		break;
	case RW1990_1: RW1990_2: TM01: 
		if (!writeRW1990_1_2_TM01(data, type)) return ERROR_COPY;
		break;
	default:
		return type;
	}
	return NOERROR;
}

void SendDallas(byte* buffer) {
	/*  iBtnEmul.init(Buffer);
	//iBtnEmul.waitForRequest(false);
	uint32_t tStart = millis();
	do {
	  if (!iBtnEmul.waitReset(10) ) continue;
	  if (!iBtnEmul.presence() ) continue;
	  if (iBtnEmul.recvAndProcessCmd() ) break;
	} while (millis() < 200 + tStart);  */
}
