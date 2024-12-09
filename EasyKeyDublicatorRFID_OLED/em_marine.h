#pragma once
byte rfidData[5];                    // значащие данные rfid em-marine
void card_number_conv(byte buf[8]) {
	rfidData[4] = (0b01111000 & buf[1]) << 1 | (0b11 & buf[1]) << 2 | buf[2] >> 6;
	rfidData[3] = (0b00011110 & buf[2]) << 3 | buf[3] >> 4;
	rfidData[2] = buf[3] << 5 | (128 & buf[4]) >> 3 | (0b00111100 & buf[4]) >> 2;
	rfidData[1] = buf[4] << 7 | (0b11100000 & buf[5]) >> 1 | 0xF & buf[5];
	rfidData[0] = (0b01111000 & buf[6]) << 1 | (0b11 & buf[6]) << 2 | buf[7] >> 6;
}

byte vertParityCheck(const byte(&buf)[8]) {  // проверка четности столбцов с данными
	if (1 & buf[7]) return ERROR_RFID_PARITY; byte k;
	k = 1 & buf[1] >> 6 + 1 & buf[1] >> 1 + 1 & buf[2] >> 4 + 1 & buf[3] >> 7 + 1 & buf[3] >> 2 + 1 & buf[4] >> 5 + 1 & buf[4] + 1 & buf[5] >> 3 + 1 & buf[6] >> 6 + 1 & buf[6] >> 1 + 1 & buf[7] >> 4;
	if (k & 1) return ERROR_RFID_PARITY;
	k = 1 & buf[1] >> 5 + 1 & buf[1] + 1 & buf[2] >> 3 + 1 & buf[3] >> 6 + 1 & buf[3] >> 1 + 1 & buf[4] >> 4 + 1 & buf[5] >> 7 + 1 & buf[5] >> 2 + 1 & buf[6] >> 5 + 1 & buf[6] + 1 & buf[7] >> 3;
	if (k & 1) return ERROR_RFID_PARITY;
	k = 1 & buf[1] >> 4 + 1 & buf[2] >> 7 + 1 & buf[2] >> 2 + 1 & buf[3] >> 5 + 1 & buf[3] + 1 & buf[4] >> 3 + 1 & buf[5] >> 6 + 1 & buf[5] >> 1 + 1 & buf[6] >> 4 + 1 & buf[7] >> 7 + 1 & buf[7] >> 2;
	if (k & 1) return ERROR_RFID_PARITY;
	k = 1 & buf[1] >> 3 + 1 & buf[2] >> 6 + 1 & buf[2] >> 1 + 1 & buf[3] >> 4 + 1 & buf[4] >> 7 + 1 & buf[4] >> 2 + 1 & buf[5] >> 5 + 1 & buf[5] + 1 & buf[6] >> 3 + 1 & buf[7] >> 6 + 1 & buf[7] >> 1;
	if (k & 1) return ERROR_RFID_PARITY;
	return NOERROR;
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

byte readEM_Marine(byte(&buf)[8], bool copykey = true) {
	uint32_t tEnd = millis();
	if (!copykey) {
		tEnd += 1000 * 60;
	} else tEnd += 64;
	byte res, bit, bitmask, ones, BYTE = 0;
again:
	for (bit = 0, bitmask = 128, ones = 0; bit < 64; bit++) {  // читаем 64 bit
		res = ttAComp();
		if (res > 1) return ERROR_RFID_TIMEOUT;
		if ((bit < 9) && (res == 0)) {			// если не находим 9 стартовых единиц - начинаем сначала
			if (millis() > tEnd) return ERROR_RFID_READ_TIMEOT;	//ti = 2;//break;//timeout
			goto again;
		} else if ((bit < 59)) {				//начиная с 9-го бита проверяем контроль четности каждой строки
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
	//включаем генератор 125кГц
	pinMode(FreqGen, OUTPUT);
	TCCR2A = TIMER2MASK; //Toggle on Compare Match on COM2A (pin 11) и счет таймера2 до OCR2A
	TCCR2B = _BV(WGM22) | _BV(CS20);								// mode 7: Fast PWM //divider 1 (no prescaling)
	OCR2A = 63;														// 63 тактов на период. Частота на COM2A (pin 11) 16000/64/2 = 125 кГц, Скважнось COM2A в этом режиме всегда 50%
	OCR2B = 31;														// Скважность COM2B 32/64 = 50%  Частота на COM2A (pin 3) 16000/64 = 250 кГц
	// включаем компаратор
	ADCSRB &= ~_BV(ACME);  // отключаем мультиплексор AC
	ACSR &= ~_BV(ACBG);    // отключаем от входа Ain0 1.1V
	delay(13);       //13 мс длятся переходные прцессы детектора
}

byte searchEM_Marine(byte(&data)[8], byte(&buf)[8],bool copyKey = true) {
	bool gr = digitalRead(G_Led); digitalWrite(G_Led, !gr);
	rfidACsetOn();  // включаем генератор 125кГц и компаратор
	byte ret = readEM_Marine(buf, copyKey);
	if (ret) {
		goto _exit;
	}
	keyType = keyEM_Marine;
	if (copyKey) {
		for (byte i = 0;;) {
			data[i] = buf[i];
			DEBUGHEX(buf[i]);
			if (++i < 8) DEBUG(':'); else break;
		}
#ifdef  DEBUG_ENABLE
		card_number_conv(buf);
		DEBUG(F(" ( ID ")); DEBUG(rfidData[4]); DEBUG(F(" data "));
		DEBUG(*(uint32_t*)(rfidData + 1));
		DEBUGLN(F(") Type: EM-Marie "));
#endif //  DEBUG_ENABLE
	}
_exit:
	if (!copyKey) TCCR2A = 0;  //Оключить ШИМ COM2A (pin 11)
	digitalWrite(G_Led, gr);
	return ret;
}

void rfidGap(unsigned int tm) {
	TCCR2A = 0;  //Оключить ШИМ COM2A
	delayMicroseconds(tm);
	TCCR2A = (TIMER2MASK);  // Включить ШИМ COM2A (pin 11)
}

void TxBitRfid(bool bit) {
	//if (data & 1) delayMicroseconds(54*8);  // 432 родные значения
	if (bit) delayMicroseconds(49 * 8);  //392 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! стало писать говеные 5577
											  //else delayMicroseconds(24*8);      // 192 родные значения
	else delayMicroseconds(19 * 8);           //152 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	rfidGap(19 * 8);                          //write gap 19
}

void TxByteRfid(byte data) {
	for (byte bitmask = 0; bitmask; bitmask <<= 1) {
		TxBitRfid(data & bitmask);
	}
}

bool T5557_blockRead(byte buf[4]) {
	if (ttAComp(2000) > 0) return false; //пишем в буфер начиная с 1-го бита 
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

void sendOpT5557(byte opCode, uint32_t password = 0, byte lockBit = 0, uint32_t data = 0, byte blokAddr = 1) {
	TxBitRfid(opCode >> 1);
	TxBitRfid(opCode & 1);  // передаем код операции 10
	//if (opCode == 0b00) return true;
	// password
	TxBitRfid(lockBit);		// lockbit 0
	for (uint32_t bitmask = (1 << 31); bitmask; bitmask >>= 1) {
		TxBitRfid(data & bitmask);
	}
	TxBitRfid(blokAddr >> 2);
	TxBitRfid(blokAddr >> 1);
	TxBitRfid(blokAddr & 1);  // адрес блока для записи
	delay(4);                 // ждем пока пишутся данные
}

byte write_rfidT5557(const byte(&data)[8], byte(&buf)[8]) {
	byte error; uint32_t data32;
	for (byte k = 0; k <= 4; k += 4) {  // send key data
		((byte*)&data32)[3] = data[k];
		((byte*)&data32)[2] = data[1 + k];
		((byte*)&data32)[1] = data[2 + k];
		((byte*)&data32)[0] = data[3 + k];
		rfidGap(30 * 8);							//start gap 30
		sendOpT5557(0b10, 0, 0, data32, k + 1);		//передаем 32 бита ключа в blok ones
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
		if (data[i] != buf[i]) { error = ERROR_COPY; goto _exit; }
	//digitalWrite(R_Led, HIGH);
_exit:
	TCCR2A = 0;  //Оключить ШИМ COM2A (pin 11)
	return error;
}

emRWType getRfidRWtype() {
	uint32_t data32, data33 = 0x148040 | (rfidUsePWD << 4);  //конфиг регистр 0b00000000000101001000000001000000;
	//rfidACsetOn();                  // включаем генератор 125кГц и компаратор
	rfidGap(30 * 8);                //start gap 30
	sendOpT5557(0b11, 0, 0, 0, 1);  //переходим в режим чтения Vendor ID
	if (!T5557_blockRead((byte*)&data32)) return Unknown;
	delay(4);
	rfidGap(20 * 8);                                                  //gap 20
	sendOpT5557(0b10, 0, 0, data33, 0);                               //передаем конфиг регистр
	delay(4);
	rfidGap(30 * 8);                //start gap 30
	sendOpT5557(0b11, 0, 0, 0, 1);  //переходим в режим чтения Vendor ID
	if (!T5557_blockRead((byte*)&data33)) return Unknown;
	sendOpT5557(0, 0, 0, 0, 0);  // send Reset
	delay(6);
	if (data32 != data33) return Unknown;
	DEBUG(F(" The rfid RW-key is T5557. Vendor ID is "));
	DEBUGHEX(data32); DEBUGLN();
	return T5557;
}

byte write_rfidem4305(byte(&data)[8]) {
	return NOERROR;
}

byte write_rfid(byte(&data)[8], byte(&buf)[8]) {
	byte error = searchEM_Marine(data, buf, false);
	if (error == NOERROR) {
		for (byte i = 0; i < 8; i++) {
			if (data[i] != buf[i]) return ERROR_SAME_KEY; // если коды совпадают, ничего писать не нужно
		}
	} else { DEBUGLN(error); return error; }
	error = getRfidRWtype();  // определяем тип T5557 (T5577) или EM4305
	switch (error) {
	case T5557: return write_rfidT5557(data, buf);
	case EM4305: return write_rfidem4305(data);
	case Unknown: break;
	}
	return ERROR_UNKNOWN_KEY;
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
