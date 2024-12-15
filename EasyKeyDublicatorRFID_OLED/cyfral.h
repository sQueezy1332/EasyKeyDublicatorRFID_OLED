#pragma once
word T1 = 0;		// Average full period log 1
word T0 = 0;		// Average full period log 0
word Ti1 = 0;		// Interval of first period - dutyLow (Cyfral) or dutyHigh (Metakom) for logical 1
word Ti0 = 0;		// Interval of first period - dutyLow (Cyfral) or dutyHigh (Metakom) for logical 0
word period = 0;	// Full period
byte dutyFirst = 0;	// Interval of low current consumption - log 1
byte dutySecond = 0;	// Interval of high current consumption -  log 0
typedef enum : uint8_t {
	NO_ERROR = 0,
	CYFRAL = 0b0001,
	METAKOM = 0b010,
	ERROR_DUTY_LOW_CYFRAL,
	ERROR_DUTY_HIGH_CYFRAL,
	ERROR_PERIOD_CYFRAL,
	ERROR_NIBBLE_CYFRAL,
	ERROR_DUTY_HIGH_METAKOM,
	ERROR_DUTY_LOW_METAKOM,
	ERROR_PERIOD_METAKOM,
	ERROR_PARITY_METAKOM,
	ERROR_VERY_LONG_LOW,
	ERROR_SYNC_BIT,
	ERROR_START_DUTY_HIGH,
	ERROR_NOT_RECOGNIZED,
} err_t;
err_t error;
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

extern bool comparator();

bool recvBitMetakom() {
	uint32_t timer = uS;
	while (comparator()) {
		if (uS - timer > 200) {
			error = ERROR_DUTY_HIGH_METAKOM;
			return false;
		}
	}
	dutyFirst = uS - timer;
	timer = uS;
	while (!comparator()) {
		if (uS - timer > 160) {
			dutySecond = 160;		//may be synchronise bit
			error = ERROR_DUTY_LOW_METAKOM;
			return false;
		}
	}
	dutySecond = uS - timer;
	if ((period = dutySecond + dutyFirst) < 50) {
		error = ERROR_PERIOD_METAKOM;
		return false;
	}
	return (dutyFirst > dutySecond);
}

bool Metakom(byte(&buf)[SIZE]) {
	register byte count1 = 0, count0 = 0, parity, i, BYTE, bitmask;
	for (i = 1; i < 5; i++) {
		for (BYTE = 0, parity = 0, bitmask = 128; bitmask; bitmask >>= 1) {
			if (recvBitMetakom()) {
last_bit:
				BYTE |= bitmask;
				T1 += period;
				Ti1 += dutyFirst;
				count1++;
				parity ^= 1;
			} else {
				if (error) {
					if ((bitmask == 1) && (i == 3)) {
						if (parity) goto last_bit;
					} else return false;
				}
				T0 += period;
				Ti0 += dutyFirst;
				count0++;
			}
		}
		if (parity) {
			error = ERROR_PARITY_METAKOM;
			return false;
		}
		buf[i] = BYTE;
	}
	//buf[4] = T1 / count1;		// division  for Period 1
	//buf[6] = Ti1 / count1;
	//if (count0) {				//checking for divider not be zero
	//	buf[5] = T0 / count0;	// division  for Period 0
	//	buf[7] = Ti0 / count0;
	//} else {
	//	buf[5] = 0;
	//	buf[7] = 0;
	//}
	return true;
}

bool Cyfral(byte(&buf)[SIZE]) {
again:
	for (byte i = 1, nibble = 0, bitmask; i < 5; ++i) {
		for (bitmask = 0b1000; bitmask; bitmask >>= 1) {
			if (!recvBitMetakom()) {
				nibble |= bitmask;
				T1 += period;
				Ti1 += dutyFirst;
			} else {
				if (error) return false;
				T0 += period;
				Ti0 += dutyFirst;
			}
		} //Serial.println(nibble, HEX);
		switch (nibble & 0xF) {
		case 0x7: case 0xB: case 0xD:case 0xE:
			if (nibble & 0xF0) {
				buf[i] = nibble;  //writing nibble from MSB
				break;
			}
			nibble <<= 4;
			continue;
		case 0x1:
			goto again;
		default:
			error = ERROR_NIBBLE_CYFRAL;
			return false;
		}
	} //fast division by 24 = (65536 / (200 * 32) - 1) * 24 == 221.76; x = (2 ^ 8) / 24 + 1 = 11,66...
	//buf[4] = (T1 * 11) >> 8;
	//buf[6] = (Ti1 * 11) >> 8;
	//buf[5] = T0 >> 3;			// division by 8
	//buf[7] = Ti0 >> 3;
	return true;
}


byte KeyDetection(byte(&buf)[SIZE]) {
	register byte startNibble, bitmask;
	word startPeriod;
	auto timerEnd = mS;
	auto timer = timerEnd;
Start:
	while (mS - timerEnd < 10) {
		error = NO_ERROR;
		if (comparator()) { continue; } //wait until high signal is start
		timerEnd += 10;
		timer = uS;		
		while (!comparator()) {			//Try read METAKOM synchronise bit log 0
			if (uS - timer > 450) {		//50 - 230 datasheet ~400 for last bit + synchro	
				//timer = mS;
				//while (!comparator()) {
				//	if (mS - timer > 1) {
				//		error = ERROR_VERY_LONG_LOW;
				//		DEBUG(error);
				//		return false;
				//	}
				//}
				error = ERROR_SYNC_BIT; DEBUG(error);
				goto Start;
			}
		}
		startPeriod = uS - timer + dutySecond;
		for (bitmask = 0b100, startNibble = 0; bitmask; bitmask >>= 1) {
			if (recvBitMetakom())
				startNibble |= bitmask;
			if (startNibble > METAKOM || error)
				goto Start;
		}
		if (startNibble == METAKOM && startPeriod + 5 >= period) {
			if (Metakom(buf)) return keyMetacom;
			DEBUG(error);
			continue;
		}//last bit could be volatile if Cyfral
		timer = uS;
		while (comparator()) {				//wait 1 duty cycle for Cyfral 8 half bits start nibble 
			if (uS - timer > 200) {
				//error = ERROR_START_DUTY_HIGH; DEBUG(error);
				goto Start;
			}
		}
		if ((uS - timer) > dutySecond || !recvBitMetakom()) {  //duty low from previos read bit Metakom or 0b0_0001
			if (Cyfral(buf)) return keyCyfral;
			DEBUG(error);
			continue;
		}
		error = ERROR_NOT_RECOGNIZED; DEBUG(error); //continue; //debug
		return false;
	}
	return false;
}
