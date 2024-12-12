/*
  Скетч к проекту "Копировальщик ключей для домофона RFID с OLED дисплеем и хранением 8 ключей в память EEPROM"
  Аппаратная часть построена на Arduino Nano
  Исходники на GitHub: https://github.com/AlexMalov/EasyKeyDublicatorRFID_OLED/
  Автор: HERR DIY, AlexMalov, 2019
  v 3.2 fix cyfral bug
*/
#include "main_header.h"

int main(void) {
	setup();
	byte error;
	for (;;) {
		switch (Serial.read()) {
		case 'e':  if (BtnErase.longPress()) {
			_OLED.print(F("The keys have been deleted"), 0, 0);
			DEBUGLN(F("The keys have been deleted"));
			EEPROM_key_count = 0;
			EEPROM_key_index = 0;
			EEPROM.update(EEPROM_KEY_COUNT, 0);
			EEPROM.update(EEPROM_KEY_INDEX, 0);
			//Sd_ReadOK();
			_OLED.update();
		}
		case 't': if (BtnOK.shortPress()) {  // переключаель режима чтение/запись
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
			if (searchEM_Marine(keyID) == NOERROR) {

			} else if (read_dallas(keyID) == NOERROR) {

			}
			/*searchKT(keyID, buffer) ||*/ // || 

				//Sd_ReadOK();
			Mode = md_read;
			digitalWrite(G_Led, HIGH);
			OLED_printKey(keyID, true);
			break;
		case md_write:
			if (keyType == keyEM_Marine) { error = write_rfid(keyID); } 
			else { error = write_ibutton(keyID); }
			OLEDprint_error(error);
			delay(1000);
			break;
		case md_blueMode:
			SendEM_Marine(keyID);
			break;
		}  //end switch

	}
	return 0;
}

void setup() {
	cli();
#ifndef _GYVERCORE_NOMILLIS
	TIMSK0 |= _BV(TOIE0);
#endif 
	sei();
	pinMode(Luse_Led, OUTPUT);
	digitalWrite(Luse_Led, HIGH);   //поменять на LOW
	_OLED.begin(SSD1306_128X32);   //инициализируем дисплей
	pinMode(BtnOKPin, INPUT_PULLUP);                            // включаем чтение и подягиваем пин кнопки режима к +5В
	pinMode(BtnUpPin, INPUT_PULLUP);
	pinMode(BtnDownPin, INPUT_PULLUP);  // включаем чтение и подягиваем пин кнопки режима к +5В
	pinMode(speakerPin, OUTPUT);
	pinMode(ACpin, INPUT);  // Вход аналогового компаратора 3В для Cyfral
	//pinMode(R_Led, OUTPUT);pinMode(G_Led, OUTPUT);pinMode(B_Led, OUTPUT);  //RGB-led
	//clearLed();
	pinMode(FreqGen, OUTPUT);
#ifdef DEBUG_ENABLE
	Serial.begin(115200);
#endif
	_OLED.clrScr();            //Очищаем буфер дисплея.
	_OLED.setFont(SmallFont);  //Перед выводом текста необходимо выбрать шрифт
	_OLED.print(F("Hello, read a key..."), LEFT, 0);
	_OLED.print(F("by sQueezy"), LEFT, 24);
	_OLED.update();
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
	} else {
		_OLED.print(F("ROM has no keys yet."), 0, 12); _OLED.update();
	}
	//enc1.setTickMode(AUTO);
	//enc1.setType(TYPE2);
	//enc1.setDirection(REVERSE);        // NORM / REVERSE
	//Timer1.initialize(1000);           // установка таймера на каждые 1000 микросекунд (= 1 мс)
	//Timer1.attachInterrupt(timerIsr);  // запуск таймера
	//digitalWrite(Luse_Led, !digitalRead(Luse_Led));
}


