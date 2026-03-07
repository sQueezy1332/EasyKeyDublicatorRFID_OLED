#pragma once
extern bool op_amp();
extern void rfid_emul_high_impl();
extern void rfid_emul_low_impl();
extern void rfid_pwm_disable();
extern void rfid_pwm_enable();
extern byte Buffer[8];

byte keyCompare(const byte [8]);
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
	const auto firstHalf = op_amp();
	auto time = uS; constexpr int timer = RFID_HALFBIT * 1.4;
	do {
		const auto secondHalf = op_amp();
		if (secondHalf != firstHalf) { 
			for (time = uS; op_amp() == secondHalf;) {
				if (uS - time > timer) break;  //1.5
			};
			return secondHalf;
		}
	} while (uS - time < timeOut);
	return ERROR_RFID_COMP_TIMEOUT;  //op_amp dont changed state
}

byte readEM_Marine(byte(&buf)[8]/*, size_t timeout = 64*/) {
	//auto timestamp = mS;
	byte bit, i, nibble, bitmask, result, par;
again:
	//extern void do_something(byte err); do_something(bit);
	//if (mS - timestamp > timeout) return bit;
	for (i = 0; i < 9; i++) { //9 ones preambula
		bit = rfid_recvbit();
		if (bit > 1) return bit;
		if (bit == 0) { 
			//bit = ERROR_RFID_HEADER; 
			goto again;
		}
	}
	for (i = 5; i != 0; --i) {	//10 nibbles of data and 10 parity row bit
		for (result = 0, nibble = 0, par = 0, bitmask = 128; nibble < 10; nibble++) {
			bit = rfid_recvbit();
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
	for (bitmask = 0b10000, result = 0; bitmask != 1 /*skip stop bit*/; bitmask >>= 1) {
		bit = rfid_recvbit();
		if (bit) {
			if (bit > 1) return bit;
			result |= bitmask;
		}
	}
	//if ((result & 1) != 0) return ERROR_RFID_STOP_BIT;
	byte _par = column_parity(buf);
	if (result != (_par << 1)) { //also stop bit check
		//bit = ERROR_RFID_PARITY_COL; 
		//goto again;
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
	//if (bit) delayMicroseconds(24 * 8 + 240);//432
	//delayMicroseconds(24*8);			//192 
	if (bit) delayMicroseconds(19 * 8 + 240);//392
	delayMicroseconds(19 * 8); //152
	rfidGap(19 * 8); //old
	
	//if (bit) delayMicroseconds(30 * 8);	//240
	//delayMicroseconds(15 * 8);		//120
	//rfidGap(32 * 8);				//write gap 32
}

void TxByteRfid(byte data) {
	for (byte bitmask = 0; bitmask; bitmask <<= 1) {
		TxBitRfid(data & bitmask);
	}
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
	rfidGap(32 * 8); //30 old
	TxBitRfid(opCode >> 1);
	TxBitRfid(opCode & 1);  // передаем код операции 10
	if (opCode == 0) return;
	// password
	TxBitRfid(lockBit);		// lockbit 0
	if (data != 0) {
		for (uint32_t bitmask = (1ul << 31); bitmask; bitmask >>= 1) { 
			TxBitRfid(data & bitmask); 
		}
	}
	for (byte bitmask = _BV(3); bitmask; bitmask >>= 1) { 
		TxBitRfid(addr & bitmask);
	}
	delay(4);              // ждем пока пишутся данные
}

byte write_T5557(const byte data[8]) {
	EM_Marine_encode(data, Buffer);
	delay(4);
	//start gap 30
	sendOpT5557(0b10, ((uint32_t*)Buffer)[1], 0x1);		//передаем 32 бита ключа в block ones
	sendOpT5557(0b10, ((uint32_t*)Buffer)[0], 0x2);
	//DEBUG('*');
	//start gap 30
	sendOpT5557(0b0);
	byte err = keyCompare(data);
	if (err) { DEBUGLN(err); }
	//digitalWrite(R_Led, HIGH);
	return err;
}

emRWType getRfidRWtype() {
	uint32_t data32, data33 = 0x148040 | (rfidUsePWD << 4);  //конфиг регистр 0b00000000000101001000000001000000;
	//rfidACsetOn();                  // включаем генератор 125кГц и компаратор
	//start gap 30
	sendOpT5557(0b11);  //переходим в режим чтения Vendor ID
	if (!T5557_blockRead(data32)) return ERROR_READ_1;
	delay(4);
	//gap 20
	sendOpT5557(0b10, data33, 0x0);                               //передаем конфиг регистр
	delay(4);
	//start gap 30
	sendOpT5557(0b11);  //переходим в режим чтения Vendor ID
	if (!T5557_blockRead(data33)) return ERROR_READ_2;
	sendOpT5557(0b00, 0x0, 0);  // send Reset
	//delay(4);
	if (data32 != data33) return Unknown;
	DEBUG(F("\nRfid RW-key is T5557. Vendor ID: "));
	DEBUGLN(data32, HEX);
	return T5557;
}

byte write_em4305(const byte(&data)[8]) {
	return NOERROR;
}

byte writeRfid(const byte(&data)[8]) {
	byte type = keyCompare(data);
	if (type == NOERROR) return KEY_SAME;// если коды совпадают, ничего писать не нужно
	if (type != KEY_MISMATCH) return type;
	type = getRfidRWtype();  // определяем тип T5557 (T5577) или EM4305
	switch (type) {
	case T5557:
		return write_T5557(data);
	case EM4305:
		return write_em4305(data);
	}
	return ERROR_UNKNOWN_KEY;
}

byte keyCompare(const byte key[8]) {
	byte i = readEM_Marine(Buffer/*, 30 * 1000*/);
	if (i == NOERROR) {
		if(memcmp(Buffer + 1, key + 1, 5)) return KEY_MISMATCH;
	}
	else return i;
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
