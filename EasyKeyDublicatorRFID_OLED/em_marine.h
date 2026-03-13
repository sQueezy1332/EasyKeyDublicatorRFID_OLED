#pragma once
extern bool op_amp();
extern void rfid_emul_high_impl();
extern void rfid_emul_low_impl();
extern void rfid_pwm_disable();
extern void rfid_pwm_enable();
extern void write_indication();
extern byte Buffer[8];

int keyCompare(const byte [8]);
byte column_parity(const byte buf[8]);

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

byte rfid_recvbit(size_t timeOut = RFID_HALFBIT * 4) {
	const bool firstHalf = op_amp();
	uint32_t time = uS; const int timer = RFID_HALFBIT * 1.4;
	do {
		const bool secondHalf = op_amp();
		if (secondHalf != firstHalf) { 
			for (time = uS; op_amp() == secondHalf;) {
				if (uS - time > timer) break;  //1.5
			};
			return secondHalf;
		}
	} while (uS - time < timeOut);
	return ERROR_RFID_COMP_TIMEOUT;  //op_amp dont changed state
}

int readEM_Marine(byte(&buf)[8]/*, size_t timeout = 64*/) {
	//auto timestamp = mS;
	byte result;
again:
	//extern void do_something(byte err); do_something(bit);
	//if (mS - timestamp > timeout) return bit;
	for (int i = 0; i < 9; i++) { //9 ones preambula
		byte bit = rfid_recvbit();
		if (bit > 1) return bit;
		if (bit == 0) { 
			//bit = ERROR_RFID_HEADER; 
			goto again;
		}
	}
	for (int i = 5; i != 0; --i) {	//10 nibbles of data and 10 parity row bit
		result = 0;
		for (byte nibble = 0, par = 0, bitmask = 128; nibble < 10; nibble++) {
			byte bit = rfid_recvbit();
			if (bit) {
				if (bit > 1) return bit;
				par ^= 1;
			}
			//if (nibble % 5 == 4) {
			if ((nibble == 5 -1) || (nibble == 10 -1)) {
				if (par) {
					//bit = ERROR_RFID_PARITY_ROW; 
					goto again;
				}
				continue;
			}
			if (bit) result |= bitmask;
			bitmask >>= 1;
		}
		buf[i] = result;
	}
	result = 0;
	for (byte bitmask = 0b10000; bitmask != 1 /*skip stop bit*/; bitmask >>= 1) {
		byte bit = rfid_recvbit();
		if (bit) {
			if (bit > 1) return bit;
			result |= bitmask;
		}
	}
	//if ((result & 1) != 0) return ERROR_RFID_STOP_BIT;
	byte _par = column_parity(buf);
	if (result != (_par << 1)) { //also stop bit check
		//bit = ERROR_RFID_PARITY_COL; 
		goto again;
		result <<= 3; result |= _par; //readed parity | calc parity
		buf[0] = result; return NOERROR;
	}
	buf[0] = 0xFF;	//em marine tag
	return NOERROR;
}

void rfidGap(size_t time) {
	rfid_pwm_disable();
	delayMicroseconds(time);
	rfid_pwm_enable(); 
}

void TxBitRfid(bool bit) {
	//if (bit) { delayMicroseconds(240); } delayMicroseconds(24*8);		//432 == 240 + 192;			
	 
	//if (bit) { delayMicroseconds(240); } delayMicroseconds(19*8);		//392 == 240 + 152;	//new !!!
	
	//rfidGap(19 * 8); //152

	if (bit) { delayMicroseconds(30 * 8); } delayMicroseconds(15 * 8);	//360 == 240 + 120;
	rfidGap(30 * 8);				//write gap 30
}

bool T5557_blockRead(uint32_t& buf) {
	byte ti = rfid_recvbit(2000); //read start 0
	if (ti != 0) return false;
	buf = 0;	//write from msb, if not find start 0 - its error
	for (uint32_t bitmask = 1ul << 31; bitmask; bitmask >>= 1) {
		if (ti = rfid_recvbit(2000)) {
			if (ti > 1) return false;  //timeout
			buf |= bitmask;
		}
	}
	return true;
}

void sendOpT5557(byte opCode, uint32_t data = 0, byte addr = 1, byte lockBit = 0, uint32_t password = 0) {
	rfidGap(30 * 8);		
	TxBitRfid(opCode >> 1);
	TxBitRfid(opCode & 1);  // передаем код операции 10
	if (opCode == 0) return;
	// password
	TxBitRfid(lockBit);		// lockbit 0
	if (data) {
		for (uint32_t bitmask = (1ul << 31); bitmask; bitmask >>= 1) { 
			TxBitRfid(data & bitmask); 
		}
	}
	for (byte bitmask = 0b100; bitmask; bitmask >>= 1) { 
		TxBitRfid(addr & bitmask);
	} 
	write_indication();
	delay(4 + 6);              // ждем пока пишутся данные
}

int write_T5557(const byte data[8]) {
	EM_Marine_encode(data, Buffer);
	delay(4);	//if (opCode == 0) return
	//start gap 30	//moved
	sendOpT5557(0b10, ((uint32_t*)Buffer)[1], 0x1);		//передаем 32 бита ключа в block ones
	//delay(6); //moved
	sendOpT5557(0b10, ((uint32_t*)Buffer)[0], 0x2);
	//delay(6); //moved
	//start gap 30 //moved
	sendOpT5557(0b00);
	return keyCompare(data);
	//digitalWrite(R_Led, HIGH);
}

emRWType getRfidRWtype() {
	const uint32_t config = 0x148040 | (rfidUsePWD << 4);  //конфиг регистр 0b00000000000101001000000001000000;
	uint32_t vendor, vendor2;
	//rfidACsetOn();                  // включаем генератор 125кГц и компаратор
	//start gap 30
	sendOpT5557(0b11);		//переходим в режим чтения Vendor ID
	if (!T5557_blockRead(vendor)) return ERROR_READ_2;
	delay(4);
	//rfidGap(20 * 8)		//gap 20
	sendOpT5557(0b10, config, 0x0);                               //передаем конфиг регистр
	//delay(4);
	//start gap 30
	sendOpT5557(0b11);		//переходим в режим чтения Vendor ID
	if (!T5557_blockRead(vendor2)) return ERROR_READ_3;
	sendOpT5557(0b00, 0x0, 0);  // send Reset
	//delay(6);
	DEBUG(F("\nVendor ID: ")); DEBUG(vendor, HEX);
	if (vendor == vendor2) {
		DEBUGLN();
		return T5557;
	}
	DEBUG(F("\tVendor2 ID: ")); DEBUGLN(vendor2, HEX);
	return UNKNOWN_TYPE;
	//DEBUGLN(F("\tRfid RW-key is T5557"));
	
}

int write_em4305(const byte(&data)[8]) {
	return NOERROR;
}

int writeRfid(const byte(&data)[8]) {
	int type = keyCompare(data);
	if (type == NOERROR)
		return KEY_SAME;// если коды совпадают, ничего писать не нужно
	if (type == KEY_MISMATCH) {
		type = getRfidRWtype();  // определяем тип T5557 (T5577) или EM4305
		switch (type) {
		case T5557:
			return write_T5557(data);
		case EM4305:
			return write_em4305(data);
		}
	}
	return type;
}

int keyCompare(const byte key[8]) {
	int ret = readEM_Marine(Buffer/*, 30 * 1000*/);
	if (ret != NOERROR) return ret;
	for (int i = 1; i < 6; i++) {
		if(Buffer[i] != key[i]) return KEY_MISMATCH;
	} 
	return NOERROR;
}

void sendEM_Marine(const byte(&buf)[8], int count = 1) {
	byte buffer[8];//FF:A9:8A:A4:87:78:98:6A //0x0000565A91B831
	EM_Marine_encode(buf, buffer); 
	while (count > 0) {
		for (int i = 7; i >= 0; --i) {
			for (byte bitmask = 128, data = buf[i]; bitmask; bitmask >>= 1) {
				if (data & bitmask) {
					rfid_emul_high_impl();
					delayMicroseconds(250);
					rfid_emul_low_impl();	//high to low
				}
				else {
					rfid_emul_low_impl();
					delayMicroseconds(250);
					rfid_emul_high_impl();	//low to high
				}
				delayMicroseconds(250);
			}
		}
		rfid_emul_high_impl();
	}
}

/*void TxByteRfid(byte data) {
	for (byte bitmask = 0; bitmask; bitmask <<= 1) {
		TxBitRfid(data & bitmask);
	}
}*/
