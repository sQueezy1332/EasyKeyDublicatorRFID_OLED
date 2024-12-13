#pragma once
extern byte buffer[8];

byte columnParity(const byte(&buf)[8]) {
	byte i = 5, result = 0, temp;
	for (; i; --i) {
		temp = buf[i];
		result ^= (temp >> 4) xor (temp);
	}
	return result & 0xF;
}

inline void rfid_disable() {
	TCCR2A = 0; pinMode(FreqGen, INPUT); // Оключить ШИМ COM2A(pin 11)
}

inline void rfid_enable() {
	pinMode(FreqGen, OUTPUT); TCCR2A = TIMER2MASK;  //Toggle on Compare Match on COM2A (pin 11) и счет таймера2 до OCR2A
}

void rfidACsetOn() { //включаем генератор 125кГц
	rfid_enable(); 
	TCCR2B = _BV(WGM22) | _BV(CS20);								// mode 7: Fast PWM //divider 1 (no prescaling)
	OCR2A = 63;														// 63 тактов на период. Частота на COM2A (pin 11) 16000/64/2 = 125 кГц, Скважнось COM2A в этом режиме всегда 50%
	OCR2B = 31;														// Скважность COM2B 32/64 = 50%  Частота на COM2A (pin 3) 16000/64 = 250 кГц
	// включаем компаратор
	ADCSRB &= ~_BV(ACME);  // отключаем мультиплексор AC
	ACSR &= ~_BV(ACBG);    // отключаем от входа Ain0 1.1V
	delay(10);       //13 мс длятся переходные процессы детектора
}

byte ttAComp(uint16_t timeOut = 7000) {  // pulse 0 or 1 or -1 if timeout
	byte AcompState, AcompInitState;
	uint32_t tEnd = micros() + timeOut;
	AcompInitState = COMPARATOR;  // читаем флаг компаратора
	do {
		AcompState = COMPARATOR;
		if (AcompState != AcompInitState) { //AcompState = COMPARATOR;  // читаем флаг компаратора
			delayMicroseconds(1000 / (rfidBitRate * 4));  // 1/4 Period on 2 kBps = 125 mks
			AcompState = COMPARATOR;               // читаем флаг компаратора
			delayMicroseconds(1000 / (rfidBitRate * 2));  // 1/2 Period on 2 kBps = 250 mks
			return AcompState ? 1 : 0;
		}
	} while (micros() < tEnd);
	return ERROR_RFID_TIMEOUT;  //таймаут, компаратор не сменил состояние
}

byte readEM_Marine(byte(&buf)[8], bool copykey = false) {
	//bool gr = digitalRead(G_Led); digitalWrite(G_Led, !gr);
	rfidACsetOn();  // включаем генератор 125кГц и компаратор
	uint32_t tEnd = millis() + (copykey ? 1000 * 30 : 64);
	byte res, i, bit, bitmask, p, BYTE;
again:
	if (millis() > tEnd) return ERROR_RFID_READ_TIMEOT;
	for (bit = 0; bit < 9; bit++) {
		res = ttAComp();
		if (res > 1) return ERROR_RFID_TIMEOUT;
		if (res == 0) goto again;
	}
	for (i = 5; i; --i) {
		for (BYTE = 0, bit = 0, p = 0, bitmask = 128; bit < 10; bit++) {
			res = ttAComp();
			if (res > 1) { return ERROR_RFID_TIMEOUT; }
			if (res == 1) p^=1;
			if ((bit == 5 - 1) || (bit == 10 - 1)) {
				if (p & 1) goto again;
				p = 0;
				continue;
			}
			if (res) BYTE |= bitmask;
			bitmask >>= 1;
		}
		buf[i] = BYTE;
	}
	for (bitmask = (1 << 5), BYTE = 0; bitmask; bitmask >>= 1) {
		res = ttAComp();
		if (res > 1) return ERROR_RFID_TIMEOUT;
		if (res) BYTE |= bitmask;
	}
	//if ((BYTE & 1) != 0) return ERROR_RFID_STOP_BIT;
	if ((BYTE >> 1) != columnParity(buf)) return ERROR_RFID_PARITY;
	buf[0] = 0xFF;	//em marine tag
	if (!copykey)rfid_disable();
	//digitalWrite(G_Led, gr);
	return NOERROR;
}

void rfidGap(uint16_t tm) {
	rfid_disable();
	delayMicroseconds(tm);
	rfid_enable();  // Включить ШИМ COM2A (pin 11)
}

void TxBitRfid(bool bit) {
	//if (bit) delayMicroseconds(54*8);  
	//if (bit) delayMicroseconds(49 * 8);	//392 - стало писать говеные 5577 // 432 родные значения
	//									//else delayMicroseconds(24*8);      // 192 родные значения
	//else delayMicroseconds(19 * 8);		//152 
	if (bit) delayMicroseconds(30 * 8);	//240
	delayMicroseconds(15 * 8);		//120
	rfidGap(30 * 8);				//write gap 32
}

void TxByteRfid(byte data) {
	for (byte bitmask = 0; bitmask; bitmask <<= 1) {
		TxBitRfid(data & bitmask);
	}
}

bool T5557_blockRead(uint32_t& buf) {
	byte ti = ttAComp(2000);
	if (ti != 0) return false; buf = 0;//пишем в буфер начиная с 1-го бита // если не находим стартовый 0 - это ошибка
	for (uint32_t bitmask = 1 << 31; bitmask; bitmask >>= 1) {  // читаем стартовый 0 и 32 значащих bit
		ti = ttAComp(2000);
		if (ti) {
			if (ti > 1) return false;  //timeout
			buf |= bitmask;
		}
	}
	return true;
}

void sendOpT5557(byte opCode, uint32_t data = 0, byte blokAddr = 1, uint32_t password = 0, byte lockBit = 0) {
	rfidGap(32 * 8);
	TxBitRfid(opCode >> 1);
	TxBitRfid(opCode & 1);  // передаем код операции 10
	if (opCode == 0) return;
	// password
	TxBitRfid(lockBit);		// lockbit 0
	if (data != 0) {
		for (uint32_t bitmask = (1 << 31); bitmask; bitmask >>= 1) { TxBitRfid(data & bitmask); }
	}
	for (byte bitmask = (1 << 3); bitmask; bitmask >>= 1) { TxBitRfid(blokAddr & bitmask); }
	delay(10);                 // ждем пока пишутся данные
}

void rfid_encode(const byte(&data)[8], byte(&buf)[8]) {
	((uint16_t*)&buf)[3] = 0xFF80;
	memset(buf, 0, 6);
	for (uint8_t b = 63 - 9, nibble = 0, BYTE, bit, parity; nibble < 10; nibble++, b--) {
		for (bit = 0, parity = 0, BYTE = 0; bit < 4; bit++, b-- ) {//read and write from msb
			if (data[5 - (((nibble << 2) + bit) >> 3)] & (128 >> (((nibble << 2) + bit) % 7))) {  //<< 2 == *4 ; >> 3 == /8; &7 == %8  
				buf[b >> 3] |= _BV(b & 7); //
				parity ^= 1;
			}
		}
		if (parity) buf[b >> 3] |= _BV(b & 0x07);
	}
	buf[0] |= columnParity(data) << 1;
}

byte write_rfidT5557(const byte(&data)[8]) {
	rfid_encode(data, buffer);// send key data
		//start gap 30
	sendOpT5557(0b10, ((uint32_t*)&buffer)[1], 0x1);		//передаем 32 бита ключа в blok ones
	sendOpT5557(0b10, ((uint32_t*)&buffer)[0], 0x2);
		//DEBUG('*');
	//start gap 30
	sendOpT5557(0);
	delay(4);
	byte i = readEM_Marine(buffer, true);
	if (i == NOERROR) {
		for (i = 5; i; --i) {
			if (data[i] != buffer[i]) { i = ERROR_COPY; break; }
		}
	}
	rfid_disable();
	//digitalWrite(R_Led, HIGH);
	return i;
}

emRWType getRfidRWtype() {
	uint32_t data32, data33 = 0x148040 | (rfidUsePWD << 4);  //конфиг регистр 0b00000000000101001000000001000000;
	//rfidACsetOn();                  // включаем генератор 125кГц и компаратор
	//start gap 30
	sendOpT5557(0b11);  //переходим в режим чтения Vendor ID
	if (!T5557_blockRead(data32)) return Unknown;
	delay(4);
	//gap 20
	sendOpT5557(0b10, data33, 0x0);                               //передаем конфиг регистр
	delay(4);
	//start gap 30
	sendOpT5557(0b11);  //переходим в режим чтения Vendor ID
	if (!T5557_blockRead(data33)) return Unknown;
	sendOpT5557(0b0, 0x0, 0);  // send Reset
	delay(4);
	if (data32 != data33) return Unknown;
	DEBUG(F(" The rfid RW-key is T5557. Vendor ID is "));
	DEBUGHEX(data32); DEBUGLN();
	return T5557;
}

byte write_rfidem4305(const byte(&data)[8]) {
	return NOERROR;
}

byte write_rfid(const byte(&data)[8]) {
	byte i = readEM_Marine(buffer, true);
	if (i == NOERROR) {
		for (i = 5;;) {
			if (data[i] != buffer[i]) break;
			if (--i == 0) return ERROR_SAME_KEY; // если коды совпадают, ничего писать не нужно
		}
	} else { DEBUGLN(i); return i; }
	i = getRfidRWtype();  // определяем тип T5557 (T5577) или EM4305
	switch (i) {
	case T5557: return write_rfidT5557(data);
	case EM4305: return write_rfidem4305(data);
	}
	return ERROR_UNKNOWN_KEY;
}

void SendEM_Marine(byte* buf) {
	rfid_disable();  // отключаем шим
	digitalWrite(FreqGen, LOW);
	//FF:A9:8A:A4:87:78:98:6A
	delay(10);
	for (byte count = 0, i, bitmask; count < 10; count++) {
		for (i = 0; i < 8; i++) {
			for (bitmask = 128; bitmask; bitmask >>= 1) {
				if (buf[i] & bitmask) {
					pinMode(FreqGen, INPUT);	//LOW
					delayMicroseconds(250);
					pinMode(FreqGen, OUTPUT);
				} else {
					pinMode(FreqGen, OUTPUT);	//HIGH
					delayMicroseconds(250);
					pinMode(FreqGen, INPUT);
				}
				delayMicroseconds(250);
			}
		}
		delay(1);
	}
	pinMode(FreqGen, INPUT);
}
