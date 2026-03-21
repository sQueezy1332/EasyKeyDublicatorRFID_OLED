/*
  Скетч к проекту "Копировальщик ключей для домофона RFID с OLED дисплеем и хранением 8 ключей в память EEPROM"
  Аппаратная часть построена на Arduino Nano
  Исходники на GitHub: https://github.com/AlexMalov/EasyKeyDublicatorRFIDOLED/
  Автор: HERR DIY, AlexMalov, 2019
  v 3.2 fix cyfral bug
*/
#include "main_header.h"
//FFAA A000 6FC2 9000
void print_key(const byte buf[]);
void do_something(byte err) {
	if (err > ERROR_RFID_HEADER) Serial.write(err);
}

__ATTR_NORETURN__ void rfid_test() {
	//*reinterpret_cast<uint64_t*>(keyID) = 0x365A1140BEFF;
	//pinMode(COMP_PIN, INPUT_PULLUP);
	pinMode(LED_BUILTIN, OUTPUT);
	pinMode(D2, OUTPUT);
	//pinMode(6, INPUT_PULLUP); //comparator invert input 0.1v
	Serial.begin(256000);
	Serial.println(__TIMESTAMP__);
	rfid_init();
	rfid_pwm_enable();
	//extern void dac_init();dac_init();
#if defined __LGT8F__
	C0SR = 0; //comparator enable
	C0XR = _BV(C0OE) | _BV(C0FEN) | 0b00; //comparator output pin D2; filter 32us (dont work)
#else
	ACSR = 0;
#endif
	byte key_buf[1][8]{};
	const byte buf_cap = sizeof key_buf / sizeof key_buf[0]; int ret;
	for (int keyIndex = 0;;) {
		uint32_t timer = millis();
		while (keyIndex < buf_cap) {
			if (!readEM_Marine(key_buf[keyIndex])) {
				if (*(uint32_t*)(key_buf[keyIndex] + 1) == 0) continue;
				++keyIndex; PINB |= _BV(PB5);
			}
		} uint32_t delta = millis() - timer;
		keyIndex = 0;
		for (int i = 0; i < buf_cap; i++) {
			print_key(key_buf[i]);
		}
		Serial.print('\t'); Serial.print(delta); //continue;
		DEBUGLN();
		delay(1000);
		ret = keyCompare(keyID);
		if (ret == NOERROR) {
			DEBUGLN("\nKEY_SAME"); continue;
		}
		else if (ret != KEY_MISMATCH) {
			DEBUGLN(ret); continue;
		}//ret = EM4305; goto write;
		ret = getRFIDtype();
		DEBUG(F("\nRFID Type: ")); DEBUGLN(ret);
		if (ret < UNKNOWN_TYPE) continue;
		DEBUG(F("Vendor IDs: ")); DEBUG(*(uint32_t*)Buffer, HEX); 
		if (ret == UNKNOWN_TYPE)
			{ DEBUG(" | "); DEBUG(*(uint32_t*)(Buffer + 4), HEX); }
		DEBUGLN();
	write:
		switch (ret) {
		case T5557:
			ret = write_T5557(keyID); break;
		case EM4305:
			ret = write_em4305(keyID); break;
		}
		//ret = writeRfid(keyID);
		switch (ret)
		{
		case NOERROR: DEBUGLN("\nSuccess"); break;
		//case KEY_SAME: DEBUGLN("\nKEY_SAME");break;
		case ERROR_RFID_COMP_TIMEOUT: DEBUGLN("\nCOMP_TIMEOUT"); continue;
		case KEY_MISMATCH: DEBUGLN("\nKEY_MISMATCH"); continue;
		//case UNKNOWN_TYPE: DEBUGLN("\nUNKNOWN_TYPE"); continue;
		default: DEBUGLN(ret);
		}
		delay(3000);
	}

}

void print_key(const byte buf[]) {
	//Serial.println();
	for (byte i = 5; i <= 5; --i) {
#ifdef ARDUINO_ARCH_ESP32
		Serial.printf("%02X ", buf[i]);
#else	
		Serial.print(' ');
		if ((buf[i] & 0xF0) == 0) Serial.print('0');
		Serial.print(buf[i], HEX);
#endif
	}
}

//int main(void) { setup();
__ATTR_NORETURN__ void loop() {
	for (int ret;;) {
		if (BtnErase.longPress()) {
			//OLED.print(F("The keys have been deleted"), 0, 0);
			DEBUGLN(F("The keys have been deleted"));
			EEPROM_key_count = 0;
			EEPROM_key_index = 0;
			EEPROM.update(EEPROM_KEY_COUNT, 0);
			EEPROM.update(EEPROM_KEY_INDEX, 0);
			//Sd_ReadOK();
			OLED.update();
		}
		if (BtnOK.shortPress()) {  // переключаель режима чтение/запись
			switch (Mode) {
			case md_empty: //Sd_ErrorBeep(); break;
			case md_read:
				Mode = md_write;
				//clearLed();
				//digitalWrite(R_Led, HIGH);
				break;
			case md_write:
				Mode = md_blueMode;
				//clearLed();
				//digitalWrite(B_Led, HIGH);
				//digitalWrite(Luse_Led, !digitalRead(Luse_Led));
				break;
			case md_blueMode:
				Mode = md_read;
				//clearLed();
				//digitalWrite(G_Led, HIGH);
				//digitalWrite(Luse_Led, !digitalRead(Luse_Led));
				break;
			}
			OLED_printKey(keyID);
			DEBUG(F("Mode: "));
			DEBUGLN(Mode);
			//Sd_WriteStep();
		}
		if (EEPROM_key_count > 0) {
			if (BtnUp.shortPress()) {  //при повороте энкодера листаем ключи из eeprom
				if (--EEPROM_key_index < 1) EEPROM_key_index = EEPROM_key_count;
				EEPROM_get_key(keyID);
				OLED_printKey(keyID);
				//Sd_WriteStep();
			} else if (BtnDown.shortPress()) {
				if (++EEPROM_key_index > EEPROM_key_count) EEPROM_key_index = 1;
				EEPROM_get_key(keyID);
				OLED_printKey(keyID);
				//Sd_WriteStep();
			}
		}
		if ((Mode != md_empty) && BtnOK.longPress()) {  // Если зажать кнопкку - ключ сохранися в EEPROM
			if (EPPROM_AddKey(keyID)) {
				OLEDprint_error(KEY_SAVED);
				//Sd_ReadOK();
				delay(1000);
			} else {//Sd_ErrorBeep();
				OLED_printKey(keyID);
			}
		}
		//else if (millis() - stTimer < 100) return;  //задержка в 100 мс
		//stTimer = millis();
		switch (Mode) {
		case md_empty: case md_read:
			if (!readEM_Marine(Buffer)) {
				keyType = keyEM_Marine;
			} else if (keyType = obj.KeyDetection(Buffer)) {
				//keyType = keyCyfral;
			} else if (read_dallas(Buffer)) {
				keyType = keyDallas;
			} else continue;

				//Sd_ReadOK();
			Mode = md_read;
			digitalWrite(G_Led, HIGH);
			OLED_printKey(keyID, true);
			break;
		case md_write:
			if (keyType == keyEM_Marine) { ret = writeRfid(keyID); }
			else { ret = write_ibutton(keyID); }
			OLEDprint_error(ret);
			delay(1000);
			break;
		case md_blueMode:
			rfid_pwm_disable();  delay(10);// отключаем шим
			sendEM_Marine(keyID);
			break;
		}  //end switch

	}
	//return 0;
}

void setup() {
	rfid_test();
/*	cli();
#ifndef _GYVERCORE_NOMILLIS
	TIMSK0 |= _BV(TOIE0);
#endif 
	sei();*/
	pinMode(Luse_Led, OUTPUT);
	digitalWrite(Luse_Led, HIGH);   //поменять на LOW
	pinMode(BtnOKPin, INPUT_PULLUP);                            // включаем чтение и подягиваем пин кнопки режима к +5В
	pinMode(BtnUpPin, INPUT_PULLUP);
	pinMode(BtnDownPin, INPUT_PULLUP);  // включаем чтение и подягиваем пин кнопки режима к +5В
	pinMode(speakerPin, OUTPUT);
	pinMode(ACpin, INPUT);  // Вход аналогового компаратора 3В для Cyfral
	//pinMode(R_Led, OUTPUT);pinMode(G_Led, OUTPUT);pinMode(B_Led, OUTPUT);  //RGB-led
	//clearLed();
	rfid_init(); //rfid_disable();
#ifdef DEBUG_ENABLE
	Serial.begin(115200);
#endif
	OLED.init();
	Wire.setClock(800000L);
	//OLED.setFont(SmallFont);  //Перед выводом текста необходимо выбрать шрифт
	//OLED.print(F("Hello, read a key..."), LEFT, 0);
	//OLED.print(F("by sQueezy"), LEFT, 24);
	//OLED.update();
	//Sd_StartOK();
	EEPROM_key_count = EEPROM[EEPROM_KEY_COUNT];
	//if (MAX_KEYS > 20) MAX_KEYS = 20;
	if (EEPROM_key_count > MAX_KEYS) EEPROM_key_count = 0;
	if (EEPROM_key_count != 0) {
		EEPROM_key_index = EEPROM[EEPROM_KEY_INDEX];
		//DEBUG(F("Read key code from EEPROM: "));
		//EEPROM_get_key(keyID);
		//for (byte i = 0;; ) {
		//	DEBUGHEX(keyID[i]); if (++i < 8)DEBUG(':'); else break;
		//}
		//DEBUGLN();
		//delay(3000);
		OLED_printKey(keyID);
		Mode = md_read;
		digitalWrite(G_Led, HIGH);
	}
	else {
		//OLED.print(F("ROM has no keys yet."), 0, 12); OLED.update();
	}
	//enc1.setTickMode(AUTO);
	//enc1.setType(TYPE2);
	//enc1.setDirection(REVERSE);        // NORM / REVERSE
	//Timer1.initialize(1000);           // установка таймера на каждые 1000 микросекунд (= 1 мс)
	//Timer1.attachInterrupt(timerIsr);  // запуск таймера
	//digitalWrite(Luse_Led, !digitalRead(Luse_Led));
}

void dac_init() {
#if defined __LGT8F__
	pinMode(5, OUTPUT); //pulldown for pin 6
	DACON = _BV(DACEN) | _BV(DAVS1) | _BV(DAOE); //DAC enable; internal reference voltage; output enable pin 4
	//VCAL = VCAL2; bitSet(ADCSRD, REFS1);//2.048v //ADC REF_V 
	//VCAL = VCAL3; bitSet(ADCSRD, REFS2); //4.096v
	//Serial.print("VCAL1 "); Serial.println(VCAL1);Serial.print("VCAL2 "); Serial.println(VCAL2);Serial.print("VCAL3 "); Serial.println(VCAL3);
	VCAL = VCAL1;
	DALR = 0xFF;
	C0SR = _BV(C0BG); //DAC output on positive input //comparator enable
#endif
}
