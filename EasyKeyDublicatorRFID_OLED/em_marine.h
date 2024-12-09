#pragma once
byte rfidData[5];                    // значащие данные rfid em-marine
void card_number_conv(byte buf[8]) {
	rfidData[4] = (0b01111000 & buf[1]) << 1 | (0b11 & buf[1]) << 2 | buf[2] >> 6;
	rfidData[3] = (0b00011110 & buf[2]) << 3 | buf[3] >> 4;
	rfidData[2] = buf[3] << 5 | (128 & buf[4]) >> 3 | (0b00111100 & buf[4]) >> 2;
	rfidData[1] = buf[4] << 7 | (0b11100000 & buf[5]) >> 1 | 0xF & buf[5];
	rfidData[0] = (0b01111000 & buf[6]) << 1 | (0b11 & buf[6]) << 2 | buf[7] >> 6;
}

byte vertParityCheck(byte* buf) {  // проверка четности столбцов с данными
	if (1 & buf[7]) return 1; byte k;
	k = 1 & buf[1] >> 6 + 1 & buf[1] >> 1 + 1 & buf[2] >> 4 + 1 & buf[3] >> 7 + 1 & buf[3] >> 2 + 1 & buf[4] >> 5 + 1 & buf[4] + 1 & buf[5] >> 3 + 1 & buf[6] >> 6 + 1 & buf[6] >> 1 + 1 & buf[7] >> 4;
	if (k & 1) return 1;
	k = 1 & buf[1] >> 5 + 1 & buf[1] + 1 & buf[2] >> 3 + 1 & buf[3] >> 6 + 1 & buf[3] >> 1 + 1 & buf[4] >> 4 + 1 & buf[5] >> 7 + 1 & buf[5] >> 2 + 1 & buf[6] >> 5 + 1 & buf[6] + 1 & buf[7] >> 3;
	if (k & 1) return 1;
	k = 1 & buf[1] >> 4 + 1 & buf[2] >> 7 + 1 & buf[2] >> 2 + 1 & buf[3] >> 5 + 1 & buf[3] + 1 & buf[4] >> 3 + 1 & buf[5] >> 6 + 1 & buf[5] >> 1 + 1 & buf[6] >> 4 + 1 & buf[7] >> 7 + 1 & buf[7] >> 2;
	if (k & 1) return 1;
	k = 1 & buf[1] >> 3 + 1 & buf[2] >> 6 + 1 & buf[2] >> 1 + 1 & buf[3] >> 4 + 1 & buf[4] >> 7 + 1 & buf[4] >> 2 + 1 & buf[5] >> 5 + 1 & buf[5] + 1 & buf[6] >> 3 + 1 & buf[7] >> 6 + 1 & buf[7] >> 1;
	if (k & 1) return 1;
	return 0;
}

byte ttAComp(uint16_t timeOut = 7000) {  // pulse 0 or 1 or -1 if timeout
	byte AcompState, AcompInitState;
	uint32_t tEnd = micros() + timeOut;
	AcompInitState = COMPARATOR;  // читаем флаг компаратора
	do {
		if (COMPARATOR != AcompInitState) { //AcompState = COMPARATOR;  // читаем флаг компаратора
			delayMicroseconds(1000 / (rfidBitRate * 4));  // 1/4 Period on 2 kBps = 125 mks
			AcompState = COMPARATOR;               // читаем флаг компаратора
			delayMicroseconds(1000 / (rfidBitRate * 2));  // 1/2 Period on 2 kBps = 250 mks
			return AcompState;
		}
	} while (micros() < tEnd);
	return 2;  //таймаут, компаратор не сменил состо€ние
}

byte readEM_Marine(byte* buf) {
	uint32_t tEnd = millis() + 50;
	byte res, bit, bitmask, ones, BYTE = 0;
again:
	for (bit = 0, bitmask = 128, ones = 0; bit < 64; bit++) {  // читаем 64 bit
		res = ttAComp();
		if (res == 2) return 2;				//timeout
		if ((bit < 9) && (res == 0)) {			// если не находим 9 стартовых единиц - начинаем сначала
			if (millis() > tEnd) return false;	//ti = 2;//break;//timeout
			goto again;
		} else if ((bit < 59)) {				//начина€ с 9-го бита провер€ем контроль четности каждой строки
			if (res) ones++;
			if ((bit - 9) % 5 == 4) {			// конец строки с данными из 5-и бит,
				if (ones & 1) goto again;		//если нечетно - начинаем сначала
				ones = 0;
			}
		}//(((18 - 9) % 5));
		if (res) BYTE |= bitmask; //if (ti) bitSet(buf[i >> 3], 7 - count);
		if ((bitmask >>= 1) == 0) {
			buf[bit >> 3] = BYTE;
			bitmask = 128;
		}
	}
	return vertParityCheck(buf);
}

void rfidACsetOn() {
	//включаем генератор 125к√ц
	pinMode(FreqGen, OUTPUT);
	TCCR2A = TIMER2MASK; //Toggle on Compare Match on COM2A (pin 11) и счет таймера2 до OCR2A
	TCCR2B = _BV(WGM22) | _BV(CS20);								// mode 7: Fast PWM //divider 1 (no prescaling)
	OCR2A = 63;														// 63 тактов на период. „астота на COM2A (pin 11) 16000/64/2 = 125 к√ц, —кважнось COM2A в этом режиме всегда 50%
	OCR2B = 31;														// —кважность COM2B 32/64 = 50%  „астота на COM2A (pin 3) 16000/64 = 250 к√ц
	// включаем компаратор
	ADCSRB &= ~_BV(ACME);  // отключаем мультиплексор AC
	ACSR &= ~_BV(ACBG);    // отключаем от входа Ain0 1.1V
}

bool searchEM_Marine(byte(&data)[8], byte(&buf)[8],bool copyKey = true) {
	bool gr = digitalRead(G_Led); digitalWrite(G_Led, !gr);
	rfidACsetOn();  // включаем генератор 125к√ц и компаратор
	delay(6);       //13 мс дл€тс€ переходные прцессы детектора
	if (readEM_Marine(buf)) {
		if (!copyKey) TCCR2A = 0;  //ќключить Ў»ћ COM2A (pin 11)
		digitalWrite(G_Led, gr);
		return false;
	}
	keyType = keyEM_Marine;
	if (copyKey) {
		for (byte i = 0;;) {
			data[i] = buf[i];
			DEBUGHEX(buf[i]);
			if (++i < 8) DEBUG(':'); else break;
		}
	}
#ifdef  DEBUG_ENABLE
	card_number_conv(buf);
	DEBUG(F(" ( ID ")); DEBUG(rfidData[4]); DEBUG(F(" data "));
	DEBUG(*(uint32_t*)(rfidData + 1));
	DEBUGLN(F(") Type: EM-Marie "));
#endif //  DEBUG_ENABLE
	if (!copyKey) TCCR2A = 0;  //ќключить Ў»ћ COM2A (pin 11)
	digitalWrite(G_Led, gr);
	return true;
}

void rfidGap(unsigned int tm) {
	TCCR2A = 0;  //ќключить Ў»ћ COM2A
	delayMicroseconds(tm);
	TCCR2A = (TIMER2MASK);  // ¬ключить Ў»ћ COM2A (pin 11)
}

void TxBitRfid(bool bit) {
	//if (data & 1) delayMicroseconds(54*8);  // 432 родные значени€
	if (bit) delayMicroseconds(49 * 8);  //392 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! стало писать говеные 5577
											  //else delayMicroseconds(24*8);      // 192 родные значени€
	else delayMicroseconds(19 * 8);           //152 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	rfidGap(19 * 8);                          //write gap 19
}

void TxByteRfid(byte data) {
	for (byte bitmask = 0; bitmask; bitmask <<= 1) {
		TxBitRfid(data & bitmask);
	}
}

bool T5557_blockRead(byte buf[4]) {
	if (ttAComp(2000) > 0) return false; //пишем в буфер начина€ с 1-го бита 
	for (byte i = 3, ti, bitmask, BYTE;;--i) {  // читаем стартовый 0 и 32 значащих bit
		for (BYTE = 0, bitmask = 128; bitmask; bitmask >>= 1) {
			ti = ttAComp(2000);
			if (ti) {
				if (ti == 2) return false;  //timeout// если не находим стартовый 0 - это ошибка
				BYTE |= bitmask;
			}
		}
		buf[i] = BYTE;
		if (i == 0) break;
	}
	return true;
}

bool sendOpT5557(byte opCode, uint32_t password = 0, byte lockBit = 0, uint32_t data = 0, byte blokAddr = 1) {
	TxBitRfid(opCode >> 1);
	TxBitRfid(opCode & 1);  // передаем код операции 10
	if (opCode == 0b00) return true;
	// password
	TxBitRfid(lockBit & 1);  // lockbit 0
	for (uint32_t bitmask = (1 << 31); bitmask; bitmask >>= 1) {
		TxBitRfid(data & bitmask);
	}
	TxBitRfid(blokAddr >> 2);
	TxBitRfid(blokAddr >> 1);
	TxBitRfid(blokAddr & 1);  // адрес блока дл€ записи
	delay(4);                 // ждем пока пишутс€ данные
	return true;
}

byte write_rfidT5557(const byte(&data)[8], byte(&buf)[8]) {
	byte error = 0;
	uint32_t data32;
	delay(6);
	for (byte k = 0; k < 2; k++) {  // send key data
		((byte*)&data32)[3] = buf[(k << 2)];
		((byte*)&data32)[2] = buf[1 + (k << 2)];
		((byte*)&data32)[1] = buf[2 + (k << 2)];
		((byte*)&data32)[0] = buf[3 + (k << 2)];
		rfidGap(30 * 8);                         //start gap 30
		sendOpT5557(0b10, 0, 0, data32, k + 1);  //передаем 32 бита ключа в blok ones
		DEBUG('*');
		delay(6);
	}
	delay(6);
	rfidGap(30 * 8);  //start gap 30
	sendOpT5557(0b00);
	delay(4);
	error = readEM_Marine(buf);
	if (error) goto _exit;
	for (byte i = 0; i < 8; i++) 
		if (data[i] != buf[i]) { error = 3; goto _exit; }
	//digitalWrite(R_Led, HIGH);
_exit:
	TCCR2A = 0;  //ќключить Ў»ћ COM2A (pin 11)
	return error;
}

emRWType getRfidRWtype() {
	uint32_t data32, data33 = 0x148040 | (rfidUsePWD << 4);  //конфиг регистр 0b00000000000101001000000001000000;
	rfidACsetOn();                  // включаем генератор 125к√ц и компаратор
	delay(13);                      //13 мс дл€тс€ переходные процессы детектора
	rfidGap(30 * 8);                //start gap 30
	sendOpT5557(0b11, 0, 0, 0, 1);  //переходим в режим чтени€ Vendor ID
	if (!T5557_blockRead((byte*)&data32)) return Unknown;
	delay(4);
	rfidGap(20 * 8);                                                  //gap 20
	sendOpT5557(0b10, 0, 0, data33, 0);                               //передаем конфиг регистр
	delay(4);
	rfidGap(30 * 8);                //start gap 30
	sendOpT5557(0b11, 0, 0, 0, 1);  //переходим в режим чтени€ Vendor ID
	if (!T5557_blockRead((byte*)&data33)) return Unknown;
	sendOpT5557(0, 0, 0, 0, 0);  // send Reset
	delay(6);
	if (data32 != data33) return Unknown;
	DEBUG(F(" The rfid RW-key is T5557. Vendor ID is "));
	DEBUGHEX(data32); DEBUGLN();
	return T5557;
}

byte write_rfidem4305(byte(&data)[8]) {
	return 0;
}

byte write_rfid(byte(&data)[8], byte(&buf)[8]) {
	if (searchEM_Marine(data, buf,false)) {
		for (byte i = 0; i < 8; i++) {
			if (data[i] != buf[i]) return 3; // если коды совпадают, ничего писать не нужно
		}
	}
	auto rwType = getRfidRWtype();  // определ€ем тип T5557 (T5577) или EM4305
	if (rwType != Unknown) DEBUG(F("\n Burning rfid ID: "));
	switch (rwType) {
	case T5557: return write_rfidT5557(data, buf);
	case EM4305: return write_rfidem4305(data);                  //пишем EM4305
	case Unknown: break;
	}
	return 0;
}

void SendEM_Marine(byte* buf) {
	TCCR2A = 0;  // отключаем шим
	digitalWrite(FreqGen, LOW);
	//FF:A9:8A:A4:87:78:98:6A
	delay(20);
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