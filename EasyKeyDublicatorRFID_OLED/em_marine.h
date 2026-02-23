#pragma once
extern bool comparator();
byte keyCompare(const byte []);
byte column_parity(const byte buf[8]);

extern void do_something(byte err);

byte column_parity(const byte buf[8]) {
	byte i = 5, result = 0, temp;
	do {
		temp = buf[i];
		result ^= (temp >> 4) xor temp;
	} while (--i);
	return result & 0xF;
}

void EM_Marine_encode(const uint8_t data[8], uint8_t buf[8]) {
	((uint32_t*)buf)[1] = 0xFF800000; ((uint32_t*)buf)[0] = 0ul;
	for (uint8_t bit = 63 - 9, nibble = 0, column, parity, mask; nibble < 10; nibble++, bit--) {
		for (column = 0, parity = 0; column < 4; column++, bit--) {//read and write from msb
			mask = ((nibble << 2) + column); //<< 2 == *4
			if (data[5 - (mask >> 3)] & (128 >> (mask & 7))) {
				buf[bit >> 3] |= _BV(bit & 7); //>> 3 == /8;  &7 == %8 
				parity ^= 1;
			}
		}
		if (parity) buf[bit >> 3] |= _BV(bit & 7);
	}
	buf[0] |= column_parity(data) << 1;
}

void EM_Marine_decode(const uint8_t data[8], uint8_t buf[8]) {
	*(uint16_t*)(&buf[6]) = 0; buf[0] = 0xFF;//EM-MARINE TAG
	for (uint8_t bit = 63 - 9, i = 5, column, result, bitmask; i > 0; --i) {
		for (column = 0, result = 0, bitmask = 128; column < 10; column++, bit--) {//read and write from msb
			if ((column == 5 - 1) || (column == 10 - 1)) continue; // (column % 5 == 4) 
			if (data[bit >> 3] & _BV(bit & 7)) {  //<< 2 == *4 ; >> 3 == /8; &7 == %8  
				result |= bitmask;
			}
			bitmask >>= 1;
		}
		buf[i] = result;
	}
}

inline void rfid_disable() {
	TCCR2A = 0; pinMode(FreqGen, INPUT); // Оключить ШИМ COM2A(pin 11)
}

inline void rfid_enable() {
	pinMode(FreqGen, OUTPUT); TCCR2A = TIMER2MASK;  //Toggle on Compare Match on COM2A (pin 11) и счет таймера2 до OCR2A
}

void rfid_init() { //включаем генератор 125кГц
	rfid_enable();
	TCCR2B = _BV(WGM22) | _BV(CS20);								// mode 7: Fast PWM //divider 1 (no prescaling)
	OCR2A = 63;														// 63 тактов на период. Частота на COM2A (pin 11) 16000/64/2 = 125 кГц, Скважнось COM2A в этом режиме всегда 50%
	//OCR2B = 31;														// Скважность COM2B 32/64 = 50%  Частота на COM2A (pin 3) 16000/64 = 250 кГц
	// включаем компаратор
	ADCSRB &= ~_BV(ACME);  // отключаем мультиплексор AC
	ACSR &= ~_BV(ACBG);    // отключаем от входа Ain0 1.1V
	delay(10);       //13 мс длятся переходные процессы детектора
}

byte rfid_recvbit(size_t timeOut = RFID_HALFBIT * 4) {
	const auto firstHalf = comparator(); 
	auto time = uS;
	do {
		const auto secondHalf = comparator();
		if (secondHalf != firstHalf) { 
			for (time = uS; comparator() == secondHalf;) {
				if (uS - time > int(RFID_HALFBIT * 1.5)) break;
			};
			return !secondHalf;
		}
	} while (uS - time < timeOut);
	return ERROR_RFID_COMP_TIMEOUT;  //comparator dont changed state
}

byte readEM_Marine(byte(&buf)[8]/*, size_t timeout = 64*/) {
	//auto timestamp = mS;
	byte bit = NOERROR, i, nibble, bitmask, par, result;
again:
	 { do_something(bit); }
	//if (mS - timestamp > timeout) return bit;
	for (i = 0; i < 9; i++) { //9 ones preambula
		bit = rfid_recvbit();
		if (bit > 1) return bit;
		if (bit == 0) { 
			bit = ERROR_RFID_HEADER; 
			goto again;
		}
	}
	for (i = 5; i; --i) {	//10 nibbles of data and 10 parity row bit
		for (result = 0, nibble = 0, par = 0, bitmask = 128; nibble < 10; nibble++) {
			bit = rfid_recvbit();
			if (bit) {
				if (bit > 1) return bit;
				par ^= 1;
			}
			//if (nibble % 5 == 4) {
			if ((nibble == 5 -1) || (nibble == 10 -1)) {
				if (par) {
					bit = ERROR_RFID_PARITY_ROW; 
					goto again;
				}
				continue;
			}
			if (bit) result |= bitmask;
			bitmask >>= 1;
		}
		buf[i] = result;
	}
	for (bitmask = _BV(5-1), result = 0; bitmask; bitmask >>= 1) {//column parity and stop bit
		bit = rfid_recvbit();
		if (bit) {
			if (bit > 1) return bit;
			result |= bitmask;
		}
	}
	//if ((result & 1) != 0) return ERROR_RFID_STOP_BIT;
	if (result != (column_parity(buf)) >> 1) { //also stop bit check
		//bit = ERROR_RFID_PARITY_COL; 
		//goto again;
		buf[0] = result; return NOERROR;
	}
	buf[0] = 0xFF;	//em marine tag
	return NOERROR;
}

void rfidGap(uint16_t time) {
	rfid_disable();
	delayMicroseconds(time);
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
	byte ti = rfid_recvbit(2000);
	if (ti != 0) return false; 
	buf = 0;//пишем в буфер начиная с 1-го бита // если не находим стартовый 0 - это ошибка
	for (uint32_t bitmask = 1ul << 31; bitmask; bitmask >>= 1) {  // читаем стартовый 0 и 32 значащих bit
		ti = rfid_recvbit(2000);
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
		for (uint32_t bitmask = (1ul << 31); bitmask; bitmask >>= 1) { TxBitRfid(data & bitmask); }
	}
	for (byte bitmask = _BV(3); bitmask; bitmask >>= 1) { TxBitRfid(blokAddr & bitmask); }
	delay(10);                 // ждем пока пишутся данные
}

byte write_T5557(const byte data[8]) {
	//start gap 30
	sendOpT5557(0b10, ((uint32_t*)&data)[1], 0x1);		//передаем 32 бита ключа в block ones
	sendOpT5557(0b10, ((uint32_t*)&data)[0], 0x2);
	//DEBUG('*');
	//start gap 30
	sendOpT5557(0b0);
	delay(4);
	if (keyCompare(data) != SAME_KEY) return ERROR_COPY;
	//digitalWrite(R_Led, HIGH);
	return NOERROR;
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

byte write_em4305(const byte(&data)[8]) {
	return NOERROR;
}

byte writeRfid(const byte(&data)[8]) {
	byte i = keyCompare(data);
	if (i == SAME_KEY) return SAME_KEY;// если коды совпадают, ничего писать не нужно
	i = getRfidRWtype();  // определяем тип T5557 (T5577) или EM4305
	switch (i) {
	case T5557: { 
		EM_Marine_encode(data, buffer);
		return write_T5557(buffer);
	}
	case EM4305: { return write_em4305(data); }
	}
	return ERROR_UNKNOWN_KEY;
}

byte keyCompare(const byte key[8]) {
	byte buffer[8];
	byte i = readEM_Marine(buffer/*, 30 * 1000*/);
	if (i == NOERROR) {
		for (i = 5;; --i) {
			if (buffer[i] != key[i]) break;
			if (i == 1) return SAME_KEY;
		}
	}
	else { DEBUGLN(i); }
	return i;
}

void sendEM_Marine(const byte(&data)[8]) {
	byte buffer[8];
	rfid_disable();  // отключаем шим
	digitalWrite(FreqGen, LOW);
	//FF:A9:8A:A4:87:78:98:6A //0x0000565A91B831
	EM_Marine_encode(data, buffer); delay(10);
	for (byte nibble = 0, i, bitmask; nibble < 10; nibble++) {
		for (i = 7;i;) {
			for (bitmask = 128, --i; bitmask; bitmask >>= 1) {
				if (data[i] & bitmask) {
					pinMode(FreqGen, INPUT);
					delayMicroseconds(255);
					pinMode(FreqGen, OUTPUT);	//high to low
				}
				else {
					pinMode(FreqGen, OUTPUT);	
					delayMicroseconds(255);
					pinMode(FreqGen, INPUT);	//low to high
				}
				delayMicroseconds(255);
			}
			if (i == 0) break;
		}
	}
	pinMode(FreqGen, INPUT);
}
