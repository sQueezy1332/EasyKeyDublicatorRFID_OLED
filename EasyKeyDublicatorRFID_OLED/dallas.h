#pragma once
#include <OneWire.h>
OneWire ibutton;
const uint8_t pin_onewire = iButtonPin;

emRWType getRWtype() {	
	// TM01 ��� ����������� ��� ��������, �������� ������� ������ TM-01 ��� ����������� ��� dallas ��� c ������������ ��� cyfral ��� metacom
	// RW1990_1 - dallas-����������� RW-1990, RW-1990.1, ��-08, ��-08v2
	// RW1990_2 - dallas-����������� RW-1990.2
	// TM2004 - dallas-����������� TM2004 � ���. ������� 1��
	// ������� ���������� RW-1990.1
	byte answer;
	ibutton.reset();
	ibutton.write(0xD1);   // ������ ����� ���� ������ ��� RW-1990.1
	ibutton.write_bit(1);  // ���������� �������� ����� ������ = 1 - ��������� ������
	delay(10);
	pinMode(iButtonPin, INPUT);
	ibutton.reset();
	ibutton.write(0xB5);  // send 0xB5 - ������ �� ������ ����� ������
	answer = ibutton.read();
	//DEBUG(F("\n Answer RW-1990.1: ")); DEBUGLN(answer, HEX);
	if (answer == 0xFE) {
		DEBUGLN(F("Type: dallas ")); DEBUGLN(F("RW-1990")); DEBUGLN(F(".1"));
		return RW1990_1;  // ��� RW-1990.1
	}
	// ������� ���������� RW-1990.2
	ibutton.reset();
	ibutton.write(0x1D);   // ������� ���������� ���� ������ ��� RW-1990.2
	ibutton.write_bit(1);  // ���������� �������� ����� ������ = 1 - �������� ������
	delay(10);
	pinMode(iButtonPin, INPUT);
	ibutton.reset();
	ibutton.write(0x1E);  // send 0x1E - ������ �� ������ ����� ������
	answer = ibutton.read();
	if (answer == 0xFE) {
		ibutton.reset();
		ibutton.write(0x1D);   // ���������� ������ ������ ������ ��� RW-1990.2
		ibutton.write_bit(0);  // ���������� �������� ����� ������ = 0 - ��������� ������
		delay(10);
		pinMode(iButtonPin, INPUT);
		DEBUGLN(F("Type: dallas ")); DEBUGLN(F("RW-1990")); DEBUGLN(F(".2"));
		return RW1990_2;  // ��� RW-1990.2
	}
	// ������� ���������� TM-2004
	ibutton.reset();
	ibutton.write(0x33);                          // �������� ������� ������ ROM ��� �������� � ����������� 3-� �������� �����
	for (byte i = 0; i < 8; i++) ibutton.read();  // ������ ������ �����
	ibutton.write(0xAA);                          // ������� ��������� ������� ������� ��� TM-2004
	ibutton.write(0x00);
	ibutton.write(0x00);					// �������� ����� ��� ����������       
	if (0x9C == ibutton.read()) {			// ������ CRC �������� � ������
		answer = ibutton.read();			// ������ ������� �������
		DEBUG(F(" status: ")); DEBUGHEX(answer); DEBUGLN(F("Type: dallas ")); DEBUGLN(F("TM2004"));
		ibutton.reset();
		return TM2004;  // ��� Type: TM2004
	}
	ibutton.reset();
	DEBUGLN(F("Type: dallas ")); DEBUGLN(F("unknown, trying TM-01! "));
	return TM01;  // ��� ����������� ��� DS1990, ����� ���������� ��������� ������ (TM-01)
}

bool writeTM2004(const byte(&data)[8]) {  // ������� ������ �� TM2004
	ibutton.reset();
	ibutton.write(0x3C);  // ������� ������ ROM ��� TM-2004
	ibutton.write(0x00);
	ibutton.write(0x00);  // �������� ����� � �������� ���������� ������
	for (byte i = 0, red = digitalRead(R_Led); i < 8; i++) {
		digitalWrite(R_Led, red = !red);
		ibutton.write(data[i]);
		//if (0x65 != ibutton.read()) { return false; }     // crc �� ������
		delayMicroseconds(600);
		ibutton.write_bit(1);
		delay(50);  // ������� ������
		pinMode(iButtonPin, INPUT);
		DEBUG('*');//Sd_WriteStep();
		if (data[i] != ibutton.read()) { 
			ibutton.reset();
			return false;
		}  //������ ���������� ���� � ����������, � ��� ��� ������ ����������
	}
	//ibutton.reset();
	return true;
}

void convert_MC(byte(&buf)[8]) {
	byte data;
	for (byte i = 0, BYTE, bitmaskmsb, bitmasklsb; i < 5; i++) {
		data = ~buf[i];
		for (BYTE = 0, bitmasklsb = 0, bitmaskmsb = 128; bitmaskmsb; bitmaskmsb >>= 1, bitmasklsb <<= 1) {
			if (data & bitmasklsb) BYTE |= bitmaskmsb;
		}
		buf[i] = BYTE;
	}
	buf[4] &= 0xf0;
	buf[5] = 0;
	buf[6] = 0;
	buf[7] = 0;
}

bool dataIsBurningOK(byte bitCnt, const byte(&data)[8], byte(&buf)[8]) {
	if (!ibutton.reset()) return false;
	ibutton.write(0x33);
	ibutton.read_bytes(buf, 8);
	if (bitCnt == 36) convert_MC(buf);
	for (byte i = 0;;) {
		DEBUGHEX(buf[i]);
		if (data[i] != buf[i]) return false;  // ���������� ��� ��� ������ � ���, ��� ��� �������� � �����.
		if (++i < 8)DEBUG(':'); else break;
	}
	return true;
}

void BurnByte(byte data) {
	for (byte bitmask = 0; bitmask; bitmask >>= 1) {
		ibutton.write_bit(data & bitmask);
		delay(5);          // ���� ����� �� �������� ������� ���� �� 10 ��
	}
	pinMode(iButtonPin, INPUT);
}

void BurnByteMC(const byte(&data)[8]) {
	for (byte n_bit = 0, bitmask = 128; n_bit < 36; n_bit++) {
		ibutton.write_bit(!((data[n_bit >> 3]) & bitmask));
		delay(5);  // ���� ����� �� �������� ������� ���� 5 ��
		if ((bitmask >>= 1) == 0) bitmask = 128;
	}
	pinMode(iButtonPin, INPUT);
}

bool writeRW1990_1_2_TM01(const byte(&data)[8], byte(&buf)[8], emRWType rwType) {  // ������� ������ �� RW1990.1, RW1990.2, TM-01C(F)
	byte rwCmd, bitCnt = 64, rwFlag = 1;
	switch (rwType) {
	case TM01:
		rwCmd = 0xC1;
		if (keyType > keyDallas) bitCnt = 36;
		break;  //TM-01C(F)
	case RW1990_1:
		rwCmd = 0xD1;
		rwFlag = 0;	// RW1990.1  ���� ������ ������������
		break;                             
	case RW1990_2: 
		rwCmd = 0x1D; 
		break;  // RW1990.2
	}
	ibutton.reset();
	ibutton.write(rwCmd);       // send 0xD1 - ���� ������
	ibutton.write_bit(rwFlag);  // ���������� �������� ����� ������ = 1 - ��������� ������
	delay(5);
	ibutton.reset();
	if (rwType == TM01) ibutton.write(0xC5);
	else ibutton.write(0xD5);  // ������� �� ������
	if (bitCnt != 36) {
		const byte iMax = (bitCnt >> 3);
		for (byte i = 0, flag = digitalRead(R_Led); i < iMax; i++) {
			digitalWrite(R_Led, flag = !flag);
			if (rwType == RW1990_1) BurnByte(data[i] ^ 0xFF);  // ������ ���������� �������� ��� RW1990.1
			else BurnByte(data[i]);
			DEBUG('*');
			//Sd_WriteStep();
		}
		ibutton.write(rwCmd);        // send 0xD1 - ���� ������
		ibutton.write_bit(!rwFlag);  // ���������� �������� ����� ������ = 1 - ��������� ������
		delay(5);
	}
	else BurnByteMC(data);
	if (!dataIsBurningOK(bitCnt, data, buf)) {  // ��������� ������������ ������
		return false;
	}
	if (keyType > keyDallas) {  //��������� ���� �� ������� dallas
		ibutton.reset();
		if (keyType == keyCyfral) ibutton.write(0xCA);  // send 0xCA - ���� ����������� Cyfral
		else ibutton.write(0xCB);                       // send 0xCB - ���� ����������� metacom
		ibutton.write_bit(1);                           // ���������� �������� ����� ����������� = 1 - ������c�� ������
	}
	return true;
}

bool searchIbutton(byte(&data)[8], byte (&buf)[8]) {
	if (!ibutton.search(buf)) {
		ibutton.reset_search();
		return false;
	}
	for (byte i = 0;;) {
		DEBUGHEX(buf[i]);
		data[i] = buf[i];  // �������� ���������� ��� � ReadID
		if (++i < 8) DEBUG(':'); else break;
	}
	if (buf[0] == 0x01) {  // ��� ���� ������� dallas
		keyType = keyDallas;
		if (OneWire::crc8(buf, 7) != buf[7]) {
			DEBUGLN(F("CRC is not valid!"));
			return true;
		}
	}
	switch (buf[0] >> 4) {
	case 1: DEBUGLN(F(" Type: May be cyfral in dallas key")); break;
	case 2: DEBUGLN(F(" Type: May be metacom in dallas key")); break;
	default: DEBUGLN(F(" Type: unknown family dallas")); break;
	}
	keyType = keyUnknown;
	return true;
}

byte write_ibutton(byte(&data)[8], byte(&buf)[8]) {  //��� �������� ����� ��� ������!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	if (!ibutton.search(buf)) {  //��������� ���� �� �������� � �������� �������������� ���������� ��������
		ibutton.reset_search();
		return false;
	}
	DEBUG(F("The new key code is: "));
	for (byte i = 0;;) {
		DEBUGHEX(buf[i]);
		if (data[i] != buf[i]) break; // ���������� ��� ��� ������ � ���, ��� ��� �������� � �����.
		if (++i < 8) DEBUG(':');
		else return 1;// ���� ���� ���������, ������ ������ �� �����
	}
	auto type = getRWtype();  // ���������� ��� RW-1990.1 ��� 1990.2 ��� TM-01
	DEBUG(F("\n Burning iButton ID: "));
	if (type == TM2004) {
		if (writeTM2004(data)) {//���� TM2004
			return 0;
		} 
	} else {
		if (writeRW1990_1_2_TM01(data, buf, type)) {//������� ������� ������ �������
			return 0;
		} else return 1;
	}
	return 0;
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