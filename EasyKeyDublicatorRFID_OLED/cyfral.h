#pragma once
uint32_t pulseACompA(bool pulse, byte Average = 80, uint32_t timeOut = 1500) {  // pulse HIGH or LOW
	bool AcompState;
	uint32_t tEnd = micros() + timeOut;
	do {
		ADCSRA |= (1 << ADSC);
		while (ADCSRA & (1 << ADSC));  // Wait until the ADSC bit has been cleared
		if (ADCH > 200) return 0;
		if (ADCH > Average) AcompState = HIGH;  // ÷èòàåì ôëàã êîìïàðàòîðà
		else AcompState = LOW;
		if (AcompState == pulse) {
			tEnd = micros() + timeOut;
			do {
				ADCSRA |= (1 << ADSC);
				while (ADCSRA & (1 << ADSC));	// Wait until the ADSC bit has been cleared
				if (ADCH > Average) AcompState = HIGH;  // ÷èòàåì ôëàã êîìïàðàòîðà
				else AcompState = LOW;
				if (AcompState != pulse) return (uint32_t)(micros() + timeOut - tEnd);
			} while (micros() < tEnd);
			return 0;  //òàéìàóò, èìïóëüñ íå âåðíóñÿ îðàòíî
		}            // end if
	} while (micros() < tEnd);
	return 0;
}

void ADCsetOn() {
	ADMUX = (ADMUX & 0b11110000) | 0b0011 | (1 << ADLAR);                // (1 << REFS0);          // ïîäêëþ÷àåì ê AC Ëèíèþ A3 ,  ëåâîå âûðàâíèå, èçìåðåíèå äî Vcc
	ADCSRB = (ADCSRB & 0b11111000) | (1 << ACME);                        // èñòî÷íèê ïåðåçàïóñêà ADC FreeRun, âêëþ÷àåì ìóëüòèïëåêñîð AC
	ADCSRA = (ADCSRA & 0b11111000) | 0b011 | (1 << ADEN) | (1 << ADSC);  // | (1<<ADATE);      // 0b011 äåëèòåëü ñêîðîñòè ADC, // âêëþ÷àåì ADC è çàïóñêàåì ADC è autotriger ADC
}

void ACsetOn() {
	ACSR |= 1 << ACBG;                      // Ïîäêëþ÷àåì êî âõîäó Ain0 1.1V äëÿ Cyfral/Metacom
	ADCSRA &= ~(1 << ADEN);                 // âûêëþ÷àåì ADC
	ADMUX = (ADMUX & 0b11110000) | 0b0011;  // ïîäêëþ÷àåì ê AC Ëèíèþ A3
	ADCSRB |= 1 << ACME;                    // âêëþ÷àåì ìóëüòèïëåêñîð AC
}

byte calcAverage(byte& halft) {
	unsigned int sum = 0;
	byte preADCH = 0, j = 0;
	for (byte i = 0; i < 0xFF; i++) {
		ADCSRA |= _BV(ADSC);
		while (ADCSRA & (1 << ADSC)) {};  // Wait until the ADSC bit has been cleared
		sum += ADCH;
		delayMicroseconds(10);
	}
	sum >>= 8;
	uint32_t tSt = micros();
	for (byte i = 0; i < 255; i++) {
		delayMicroseconds(4);
		ADCSRA |= _BV(ADSC);
		while (ADCSRA & _BV(ADSC)); {} // Wait until the ADSC bit has been cleared
		if (((ADCH > sum) && (preADCH < sum)) || ((ADCH < sum) && (preADCH > sum))) {
			j++;
			preADCH = ADCH;
		}
	}
	halfT = (byte)((micros() - tSt) / j);
	return (byte)sum;
}

bool read_cyfral(byte* buf) {
	uint32_t ti;
	byte i = 0, j = 0, k = 0;
	analogRead(iButtonPin);
	ADCsetOn();
	byte aver = calcAverage(halfT);
	uint32_t tEnd = millis() + 30;
	do {
		ti = pulseACompA(HIGH, aver);
		if ((ti == 0) || (ti > 260) || (ti < 10)) {
			i = j = k = 0;
			continue;
		}
		if ((i < 3) && (ti > halfT)) {
			i = 0;
			j = 0;
			k = 0;
			continue;
		}  //êîíòðîëü ñòàðòîâîé ïîñëåäîâàòåëüíîñòè 0b0001
		if ((i == 3) && (ti < halfT)) continue;
		if (ti > halfT) bitSet(buf[i >> 3], 7 - j);
		else if (i > 3) k++;
		if ((i > 3) && ((i - 3) % 4 == 0)) {  //íà÷èíàÿ ñ 4-ãî áèòà ïðîâåðÿåì êîëè÷åñòâî íóëåé êàæäîé ñòðîêè èç 4-è áèò
			if (k != 1) {
				for (byte n = 0; n < (i >> 3) + 2; n++) buf[n] = 0;
				i = j = k = 0;
				continue;
			}  //åñëè íóëåé áîëüøå îäíîé - íà÷èíàåì ñíà÷àëà
			k = 0;
		}
		j++;
		if (j > 7) j = 0;
		i++;
	} while ((millis() < tEnd) && (i < 36));
	if (i < 36) return false;
	return true;
}

bool searchCyfral() {
	for (byte i = 0; i < 8; i++) { addr[i] = 0; keyID[i] = 0; }
	if (!read_cyfral(addr) || !read_cyfral(keyID)) return false;
	for (byte i = 0;;) {
		if (addr[i] != keyID[i]) return false;
		DEBUGHEX(addr[i]);
		if (++i < 8)DEBUG(':'); else break;
		//keyID[i] = addr[i];  // êîïèðóåì ïðî÷òåííûé êîä â ReadID
	}
	keyType = keyCyfral;
	DEBUGLN(F(" Type: Cyfral "));
	return true;
}

bool read_metacom(byte* buf) {
	uint32_t ti;
	byte i = 0, j = 0, k = 0;
	analogRead(iButtonPin);
	ADCsetOn();
	byte aver = calcAverage(halfT);
	uint32_t tEnd = millis() + 30;
again:
	for (i = 0, j = 0, k = 0; i < 36; i++) {
		ti = pulseACompA(LOW, aver);
		if ((ti == 0) || (ti > 500)) {
			goto again;
		}
		if ((i == 0) && (ti + 30 < (halfT << 1))) continue;  //âû÷èñëÿåì ïåðèîä;
		if ((i == 2) && (ti > halfT)) {
			goto again;
		}  //âû÷èñëÿåì ïåðèîä;
		if (((i == 1) || (i == 3)) && (ti < halfT)) {
			goto again;
		}  //âû÷èñëÿåì ïåðèîä;
		if (ti < halfT) {
			buf[i >> 3] |= _BV(7 - j);
			if (i > 3) k++;  // ñ÷èòàåì êîë-âî åäèíèö
		}
		if ((i > 3) && ((i - 3) & 7 == 0)) {  //íà÷èíàÿ ñ 4-ãî áèòà ïðîâåðÿåì êîíòðîëü ÷åòíîñòè êàæäîé ñòðîêè èç 8-è áèò
			if (k & 1) {
				for (byte n = 0; n < (i >> 3) + 1; n++) buf[n] = 0;
				goto again;
			}  //åñëè íå÷åòíî - íà÷èíàåì ñíà÷àëà
			k = 0;
		}
		if (++j > 7) j = 0;
		if (millis() > tEnd) return false;
	}
	return true;
}

bool searchKT(byte(&data)[8], byte(&buf)[8]) {
	for (byte i = 0; i < 8; i++) { buf[i] = 0; data[i] = 0; }
	if (read_cyfral(buf) && read_cyfral(data)) keyType = keyCyfral;
	else if (read_metacom(buf) && read_metacom(data)) keyType = keyMetacom;
	else return false;
	DEBUG(F("Type: ")); DEBUGLN(keyType);
	for (byte i = 0;;) {
		if (buf[i] != data[i]) return false;
		DEBUGHEX(buf[i]);
		if (++i < 8)DEBUG(':'); else break;
		//keyID[i] = addr[i];  // êîïèðóåì ïðî÷òåííûé êîä â ReadID
	}
	return true;
}
