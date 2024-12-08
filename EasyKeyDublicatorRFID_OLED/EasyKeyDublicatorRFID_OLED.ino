/*
  Скетч к проекту "Копировальщик ключей для домофона RFID с OLED дисплеем и хранением 8 ключей в память EEPROM"
  Аппаратная часть построена на Arduino Nano
  Исходники на GitHub: https://github.com/AlexMalov/EasyKeyDublicatorRFID_OLED/
  Автор: HERR DIY, AlexMalov, 2019
  v 3.2 fix cyfral bug
*/

#include <OneWire.h>
//#include <OneWireSlave.h>
#include "pitches.h"
#include <EEPROM.h>
#include <OLED_I2C.h>
OLED _OLED(SDA, SCL);  //создаем экземпляр класса OLED с именем myOLED
//typedef uint8_t byte;
extern uint8_t SmallFont[];
extern uint8_t BigNumbers[];

//#include "GyverEncoder.h"
#include "DualFunctionButton.h"	
#include "TimerOne.h"

#define DEBUG_ENABLE
#ifdef DEBUG_ENABLE
#define DEBUG(x) Serial.print(x)
#define DEBUGHEX(x) Serial.print(x, HEX)
#define DEBUGLN(x) Serial.println(x)

#else
#define DEBUG(x)
#define DEBUGHEX(x)
#define DEBUGLN(x)
#endif

//settings
#define rfidUsePWD 0    // ключ использует пароль для изменения
#define rfidPWD 123456  // пароль для ключа
#define rfidBitRate 2   // Скорость обмена с rfid в kbps
#define COMPARATOR (ACSR & _BV(ACO))

//pins
#define iButtonPin A3   // Линия data ibutton
//#define iBtnEmulPin A1  // Линия эмулятора ibutton
#define Luse_Led 13     // Светодиод лузы
#define R_Led 2         // RGB Led
#define G_Led 3
#define B_Led 4
#define ACpin 6        // Вход Ain0 аналогового компаратора 0.1В для EM-Marie
#define speakerPin 12  // Спикер, он же buzzer, он же beeper
#define FreqGen 11     // генератор 125 кГц
//#define CLK 8          // s1 энкодера
//#define DT 9           // s2 энкодера
#define BtnUpPin 8    // Кнопка вверх
#define BtnDownPin 9  // Кнопка вниз
#define BtnOKPin 10      // Кнопка переключения режима чтение/запись
#define EEPROM_KEY_COUNT (E2END)
#define EEPROM_KEY_INDEX (E2END - 1)
const uint8_t pin_onewire = iButtonPin;
//uint32_t stTimer;

DualFunctionButton BtnOK(BtnOKPin, 2000, INPUT_PULLUP);
DualFunctionButton BtnErase(BtnOKPin, 5000, INPUT_PULLUP);
DualFunctionButton BtnUp(BtnUpPin, 2000, INPUT_PULLUP);
DualFunctionButton BtnDown(BtnDownPin, 2000, INPUT_PULLUP);

//Encoder enc1(CLK, DT, BtnPin);
OneWire ibutton;
//OneWireSlave iBtnEmul(iBtnEmulPin);  //Эмулятор iButton для BlueMode
const byte MAX_KEYS = EEPROM.length() / 8 - 1;                   // максимальное кол-во ключей, которое влазит в EEPROM, но не > 20
byte EEPROM_key_count;               // количество ключей 0..MAX_KEYS, хранящихся в EEPROM
byte EEPROM_key_index = 0;           // 1..EEPROM_key_count номер последнего записанного в EEPROM ключа
byte addr[8];                        // временный буфер
byte keyID[8];                       // ID ключа для записи
byte rfidData[5];                    // значащие данные rfid em-marine
byte halfT;                          // полупериод для метаком
byte rom[8]{ 0x1, 0xBE, 0x40, 0x11, 0x5A, 0x36, 0x0, 0xE1 };

enum emRWType {
	rwUnknown,
	TM01,
	RW1990_1,
	RW1990_2,
	TM2004,
	T5557,
	EM4305
};  // тип болванки
enum emkeyType {
	keyUnknown,
	keyDallas,
	keyTM2004,
	keyCyfral,
	keyMetacom,
	keyEM_Marine
};  // тип оригинального ключа
emkeyType keyType;
enum emMode : uint8_t {
	md_empty,
	md_read,
	md_write,
	md_blueMode
};  // режим ра,оты копировальщика
emMode copierMode = md_empty;

void OLED_printKey(byte buf[8], byte msgType = false) {
	String str;
	if (!msgType) {
		str += "The key "; str += EEPROM_key_index; str += " of "; str += EEPROM_key_count; str += " in ROM";
	} else {
		auto index = indxKeyInROM(buf);
		if (index != 0) { str += "The key exists in ROM: "; str += index; } else { str += "Hold the Btn to save"; }
	}
	_OLED.clrScr(); _OLED.print(str, 0, 0);
	str = "";
	for (byte i = 0;;) {
		str += String(buf[i], HEX); if (++i < 8)str += ':'; else break;
	}
	_OLED.print(str, 0, 12);
	str = "Type ";
	switch (keyType) {
	case keyDallas: str += "Dallas wire"; break;
	case keyCyfral: str += "Cyfral wire"; break;
	case keyMetacom: str += "Metakom wire"; break;
	case keyEM_Marine: str += "EM_Marine rfid"; break;
	case keyUnknown: str += "Unknown"; break;
	}
	_OLED.print(str, 0, 24);
	_OLED.update();
}

void OLED_printError(String st, bool err = true) {
	DEBUGLN(st);
	_OLED.clrScr();
	if (err) _OLED.print(F("Error!"), 0, 0);
	else _OLED.print(F("OK"), 0, 0);
	_OLED.print(st, 0, 12);
	_OLED.update();
}

void setup() {
	pinMode(Luse_Led, OUTPUT);
	digitalWrite(Luse_Led, HIGH);   //поменять на LOW
	_OLED.begin(SSD1306_128X32);   //инициализируем дисплей
	pinMode(BtnOKPin, INPUT_PULLUP);                            // включаем чтение и подягиваем пин кнопки режима к +5В
	pinMode(BtnUpPin, INPUT_PULLUP);
	pinMode(BtnDownPin, INPUT_PULLUP);  // включаем чтение и подягиваем пин кнопки режима к +5В
	pinMode(speakerPin, OUTPUT);
	pinMode(ACpin, INPUT);  // Вход аналогового компаратора 3В для Cyfral
	//pinMode(R_Led, OUTPUT);
	//pinMode(G_Led, OUTPUT);
	//pinMode(B_Led, OUTPUT);  //RGB-led
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
		DEBUG(F("Read key code from EEPROM: "));
		EEPROM_get_key();
		for (byte i = 0;; ) {
			DEBUGHEX(keyID[i]); if (++i < 8)DEBUG(':'); else break;
		}
		DEBUGLN();
		//delay(3000);
		OLED_printKey(keyID);
		copierMode = md_read;
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

/*void clearLed() {
	digitalWrite(R_Led, LOW);
	digitalWrite(G_Led, LOW);
	digitalWrite(B_Led, LOW);
}*/

byte indxKeyInROM(byte buf[]) {  //возвращает индекс или ноль если нет в ROM
	uint16_t idx = 0;
	for (byte count = 1, i = 0; count <= EEPROM_key_count; count++, i = 0, idx += 8) {  // ищем ключ в eeprom.
		do {
			if (EEPROM[idx + i] != buf[i]) break;
		} while (++i < 8);
		if (i == 8) return count;
	}
	return 0;
}

bool EPPROM_AddKey(byte buf[]) {
	auto indx = indxKeyInROM(buf);  // ищем ключ в eeprom. Если находим, то не делаем запись, а индекс переводим в него
	if (indx != 0) {
		EEPROM_key_index = indx;
		EEPROM.update(EEPROM_KEY_INDEX, EEPROM_key_index);
		return false;
	}DEBUGLN(F("Adding to EEPROM\t"));
	for (byte i = 0;;) { DEBUGHEX(buf[i]); if (++i < 8)DEBUG(':'); else break; }DEBUGLN();
	if (EEPROM_key_count < MAX_KEYS) EEPROM_key_index = ++EEPROM_key_count;
	else EEPROM_key_index++; //EEPROM_key_count == MAX_KEYS
	if (EEPROM_key_index > EEPROM_key_count) EEPROM_key_index = 1;
	EEPROM.put((EEPROM_key_index - 1) * 8, buf);
	EEPROM.update(EEPROM_KEY_COUNT, EEPROM_key_count);
	EEPROM.update(EEPROM_KEY_INDEX, EEPROM_key_index);
	return true;
}

void EEPROM_get_key() {
	if (EEPROM_key_index > MAX_KEYS) return;
	EEPROM.get((EEPROM_key_index - 1) * 8, keyID);
	keyType = getKeyType(keyID);
}

emkeyType getKeyType(byte* buf) {
	if (buf[0] == 0x01) return keyDallas;  // это ключ формата dallas
	if ((buf[0] >> 4) == 1) return keyCyfral;
	if ((buf[0] >> 4) == 2) return keyMetacom;
	if ((buf[0] == 0xFF) && vertParityCheck(buf)) return keyEM_Marine;
	return keyUnknown;
}

//*************** dallas **************
emRWType getRWtype() {
	byte answer;
	// TM01 это неизвестный тип болванки, делается попытка записи TM-01 без финализации для dallas или c финализацией под cyfral или metacom
	// RW1990_1 - dallas-совместимые RW-1990, RW-1990.1, ТМ-08, ТМ-08v2
	// RW1990_2 - dallas-совместимая RW-1990.2
	// TM2004 - dallas-совместимая TM2004 в доп. памятью 1кб
	// пробуем определить RW-1990.1
	ibutton.reset();
	ibutton.write(0xD1);   // проуем снять флаг записи для RW-1990.1
	ibutton.write_bit(1);  // записываем значение флага записи = 1 - отключаем запись
	delay(10);
	pinMode(iButtonPin, INPUT);
	ibutton.reset();
	ibutton.write(0xB5);  // send 0xB5 - запрос на чтение флага записи
	answer = ibutton.read();
	//DEBUG(F("\n Answer RW-1990.1: ")); DEBUGLN(answer, HEX);
	if (answer == 0xFE) {
		DEBUGLN(F(" Type: dallas RW-1990.1 "));
		return RW1990_1;  // это RW-1990.1
	}
	// пробуем определить RW-1990.2
	ibutton.reset();
	ibutton.write(0x1D);   // пробуем установить флаг записи для RW-1990.2
	ibutton.write_bit(1);  // записываем значение флага записи = 1 - включаем запись
	delay(10);
	pinMode(iButtonPin, INPUT);
	ibutton.reset();
	ibutton.write(0x1E);  // send 0x1E - запрос на чтение флага записи
	answer = ibutton.read();
	if (answer == 0xFE) {
		ibutton.reset();
		ibutton.write(0x1D);   // возвращаем оратно запрет записи для RW-1990.2
		ibutton.write_bit(0);  // записываем значение флага записи = 0 - выключаем запись
		delay(10);
		pinMode(iButtonPin, INPUT);
		DEBUGLN(F(" Type: dallas RW-1990.2 "));
		return RW1990_2;  // это RW-1990.2
	}
	// пробуем определить TM-2004
	ibutton.reset();
	ibutton.write(0x33);                          // посылаем команду чтения ROM для перевода в расширенный 3-х байтовый режим
	for (byte i = 0; i < 8; i++) ibutton.read();  // читаем данные ключа
	ibutton.write(0xAA);                          // пробуем прочитать регистр статуса для TM-2004
	ibutton.write(0x00);
	ibutton.write(0x00);          // передаем адрес для считывания
	answer = ibutton.read();      // читаем CRC комманды и адреса
	byte m1[3] = { 0xAA, 0, 0 };  // вычисляем CRC комманды
	if (OneWire::crc8(m1, 3) == answer) {
		answer = ibutton.read();  // читаем регистр статуса
		DEBUG(" status: "); DEBUGHEX(answer);
		DEBUGLN(F(" Type: dallas TM2004"));
		ibutton.reset();
		return TM2004;  // это Type: TM2004
	}
	ibutton.reset();
	DEBUGLN(F(" Type: dallas unknown, trying TM-01! "));
	return TM01;  // это неизвестный тип DS1990, нужно перебирать алгоритмы записи (TM-01)
}

bool write2iBtnTM2004() {  // функция записи на TM2004
	byte answer;
	bool result = true;
	ibutton.reset();
	ibutton.write(0x3C);  // команда записи ROM для TM-2004
	ibutton.write(0x00);
	ibutton.write(0x00);  // передаем адрес с которого начинается запись
	for (byte i = 0; i < 8; i++) {
		//digitalWrite(R_Led, !digitalRead(R_Led));
		ibutton.write(keyID[i]);
		answer = ibutton.read();
		//if (OneWire::crc8(m1, 3) != answer){result = false; break;}     // crc не верный
		delayMicroseconds(600);
		ibutton.write_bit(1);
		delay(50);  // испульс записи
		pinMode(iButtonPin, INPUT);
		DEBUG('*');
		//Sd_WriteStep();
		if (keyID[i] != ibutton.read()) {
			result = false;
			break;
		}  //читаем записанный байт и сравниваем, с тем что должно записаться
	}
	if (!result) {
		ibutton.reset();
		DEBUGLN(F(" The key copy faild"));
		OLED_printError(F("The key copy faild"));
		////Sd_ErrorBeep();
		//digitalWrite(R_Led, HIGH);
		return false;
	}
	ibutton.reset();
	DEBUGLN(F(" The key has copied successesfully"));
	OLED_printError(F("The key has copied"), false);
	// //Sd_ReadOK();
	delay(2000);
	//digitalWrite(R_Led, HIGH);
	return true;
}

bool write2iBtnRW1990_1_2_TM01(emRWType rwType) {  // функция записи на RW1990.1, RW1990.2, TM-01C(F)
	byte rwCmd, bitCnt = 64, rwFlag = 1;
	switch (rwType) {
	case TM01:
		rwCmd = 0xC1;
		if ((keyType == keyMetacom) || (keyType == keyCyfral)) bitCnt = 36;
		break;  //TM-01C(F)
	case RW1990_1:
		rwCmd = 0xD1;
		rwFlag = 0;
		break;                             // RW1990.1  флаг записи инвертирован
	case RW1990_2: rwCmd = 0x1D; break;  // RW1990.2
	}
	ibutton.reset();
	ibutton.write(rwCmd);       // send 0xD1 - флаг записи
	ibutton.write_bit(rwFlag);  // записываем значение флага записи = 1 - разрешить запись
	delay(5);
	pinMode(iButtonPin, INPUT);
	ibutton.reset();
	if (rwType == TM01) ibutton.write(0xC5);
	else ibutton.write(0xD5);  // команда на запись
	if (bitCnt == 36) BurnByteMC(keyID);
	else
		for (byte i = 0; i < (bitCnt >> 3); i++) {
			//digitalWrite(R_Led, !digitalRead(R_Led));
			if (rwType == RW1990_1) BurnByte(~keyID[i]);  // запись происходит инверсно для RW1990.1
			else BurnByte(keyID[i]);
			DEBUG('*');
			////Sd_WriteStep();
		}
	if (bitCnt == 64) {
		ibutton.write(rwCmd);        // send 0xD1 - флаг записи
		ibutton.write_bit(!rwFlag);  // записываем значение флага записи = 1 - отключаем запись
		delay(5);
		pinMode(iButtonPin, INPUT);
	}
	//digitalWrite(R_Led, LOW);
	if (!dataIsBurningOK(bitCnt)) {  // проверяем корректность записи
		DEBUGLN(F(" The key copy faild"));
		OLED_printError(F("The key copy faild"));
		////Sd_ErrorBeep();
		//digitalWrite(R_Led, HIGH);
		return false;
	}
	if ((keyType == keyMetacom) || (keyType == keyCyfral)) {  //переводим ключ из формата dallas
		ibutton.reset();
		if (keyType == keyCyfral) ibutton.write(0xCA);  // send 0xCA - флаг финализации Cyfral
		else ibutton.write(0xCB);                       // send 0xCB - флаг финализации metacom
		ibutton.write_bit(1);                           // записываем значение флага финализации = 1 - перевезти формат
		delay(10);
		pinMode(iButtonPin, INPUT);
	}
	OLED_printError(F("The key has copied"), false);
	//Sd_ReadOK();
	delay(2000);
	// digitalWrite(R_Led, HIGH);
	return true;
}

void BurnByte(byte data) {
	for (byte n_bit = 0; n_bit < 8; n_bit++) {
		ibutton.write_bit(data & 1);
		delay(5);          // даем время на прошивку каждого бита до 10 мс
		data = data >> 1;  // переходим к следующему bit
	}
	pinMode(iButtonPin, INPUT);
}

void BurnByteMC(byte buf[8]) {
	byte j = 0;
	for (byte n_bit = 0; n_bit < 36; n_bit++) {
		ibutton.write_bit(((~buf[n_bit >> 3]) >> (7 - j)) & 1);
		delay(5);  // даем время на прошивку каждого бита 5 мс
		j++;
		if (j > 7) j = 0;
	}
	pinMode(iButtonPin, INPUT);
}

void convetr2MC(byte buf[8]) {
	byte data;
	for (byte i = 0; i < 5; i++) {
		data = ~buf[i];
		buf[i] = 0;
		for (byte j = 0; j < 8; j++)
			if ((data >> j) & 1) bitSet(buf[i], 7 - j);
	}
	buf[4] &= 0xf0;
	buf[5] = 0;
	buf[6] = 0;
	buf[7] = 0;
}

bool dataIsBurningOK(byte bitCnt) {
	byte buff[8];
	if (!ibutton.reset()) return false;
	ibutton.write(0x33);
	ibutton.read_bytes(buff, 8);
	if (bitCnt == 36) convetr2MC(buff);
	for (byte i = 0; i < 8; i++) {
		DEBUGHEX(buff[i]);
		DEBUG(":");
		if (keyID[i] != buff[i]) return false;  // сравниваем код для записи с тем, что уже записано в ключе.
	}
	return true;
}

bool write2iBtn() {  //это ожидание ключа для записи!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	byte i = 0;
	if (!ibutton.search(addr)) {  //комментим ифку со скобками и получаем принудительную перезапись далласов
		ibutton.reset_search();
		return false;
	}
	DEBUG(F("The new key code is: "));
	do {
		DEBUGHEX(addr[i]);
		DEBUG(':');
		if (keyID[i] != addr[i]) break;  // сравниваем код для записи с тем, что уже записано в ключе.
	} while (++i < 8);
	if (i < 8) {  // если коды совпадают, ничего писать не нужно
	   // digitalWrite(R_Led, LOW);
		OLED_printError(F("It is the same key"));
		//Sd_ErrorBeep();
		//digitalWrite(R_Led, HIGH);
		return false;
	}
	emRWType rwType = getRWtype();  // определяем тип RW-1990.1 или 1990.2 или TM-01
	DEBUG(F("\n Burning iButton ID: "));
	if (rwType == TM2004) return write2iBtnTM2004();  //шьем TM2004
	else return write2iBtnRW1990_1_2_TM01(rwType);    //пробуем прошить другие форматы
}

bool searchIbutton() {
	if (!ibutton.search(addr)) {
		ibutton.reset_search();
		return false;
	}
	for (byte i = 0; i < 8; i++) {
		DEBUGHEX(addr[i]);
		DEBUG(":");
		keyID[i] = addr[i];  // копируем прочтенный код в ReadID
	}
	if (addr[0] == 0x01) {  // это ключ формата dallas
		keyType = keyDallas;
		if (getRWtype() == TM2004) keyType = keyTM2004;
		if (OneWire::crc8(addr, 7) != addr[7]) {
			DEBUGLN(F("CRC is not valid!"));
			OLED_printError(F("CRC is not valid!"));
			//Sd_ErrorBeep();
			// digitalWrite(B_Led, HIGH); //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
			return false;
		}
		return true;
	}
	switch (addr[0] >> 4) {
	case 1: DEBUGLN(F(" Type: May be cyfral in dallas key")); break;
	case 2: DEBUGLN(F(" Type: May be metacom in dallas key")); break;
	case 3: DEBUGLN(F(" Type: unknown family dallas")); break;
	}
	keyType = keyUnknown;
	return true;
}

//************ Cyfral ***********************
uint32_t pulseACompA(bool pulse, byte Average = 80, uint32_t timeOut = 1500) {  // pulse HIGH or LOW
	bool AcompState;
	uint32_t tEnd = micros() + timeOut;
	do {
		ADCSRA |= (1 << ADSC);
		while (ADCSRA & (1 << ADSC))
			;  // Wait until the ADSC bit has been cleared
		if (ADCH > 200) return 0;
		if (ADCH > Average) AcompState = HIGH;  // читаем флаг компаратора
		else AcompState = LOW;
		if (AcompState == pulse) {
			tEnd = micros() + timeOut;
			do {
				ADCSRA |= (1 << ADSC);
				while (ADCSRA & (1 << ADSC))
					;                                     // Wait until the ADSC bit has been cleared
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

bool read_cyfral(byte* buf) {
	uint32_t ti;
	byte i = 0, j = 0, k = 0;
	analogRead(iButtonPin);
	ADCsetOn();
	byte aver = calcAverage();
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
		}  //контроль стартовой последовательности 0b0001
		if ((i == 3) && (ti < halfT)) continue;
		if (ti > halfT) bitSet(buf[i >> 3], 7 - j);
		else if (i > 3) k++;
		if ((i > 3) && ((i - 3) % 4 == 0)) {  //начиная с 4-го бита проверяем количество нулей каждой строки из 4-и бит
			if (k != 1) {
				for (byte n = 0; n < (i >> 3) + 2; n++) buf[n] = 0;
				i = j = k = 0;
				continue;
			}  //если нулей больше одной - начинаем сначала
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
		//keyID[i] = addr[i];  // копируем прочтенный код в ReadID
	}
	keyType = keyCyfral;
	DEBUGLN(F(" Type: Cyfral "));
	return true;
}

byte calcAverage() {
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

bool read_metacom(byte* buf) {
	uint32_t ti;
	byte i = 0, j = 0, k = 0;
	analogRead(iButtonPin);
	ADCsetOn();
	byte aver = calcAverage();
	uint32_t tEnd = millis() + 30;
again:
	for (i = 0, j = 0, k = 0; i < 36; i++) {
		ti = pulseACompA(LOW, aver);
		if ((ti == 0) || (ti > 500)) {
			goto again;
		}
		if ((i == 0) && (ti + 30 < (halfT << 1))) continue;  //вычисляем период;
		if ((i == 2) && (ti > halfT)) {
			goto again;
		}  //вычисляем период;
		if (((i == 1) || (i == 3)) && (ti < halfT)) {
			goto again;
		}  //вычисляем период;
		if (ti < halfT) {
			buf[i >> 3] |= _BV(7 - j);
			if (i > 3) k++;  // считаем кол-во единиц
		}
		if ((i > 3) && ((i - 3) & 7 == 0)) {  //начиная с 4-го бита проверяем контроль четности каждой строки из 8-и бит
			if (k & 1) {
				for (byte n = 0; n < (i >> 3) + 1; n++) buf[n] = 0;
				goto again;
			}  //если нечетно - начинаем сначала
			k = 0;
		}
		if (++j > 7) j = 0;
		if (millis() > tEnd) return false;
	}
	return true;
}

bool searchKT() {
	for (byte i = 0; i < 8; i++) { addr[i] = 0; keyID[i] = 0; }
	if (read_cyfral(addr) && read_cyfral(keyID)) keyType = keyCyfral;
	else if (read_metacom(addr) && read_metacom(keyID)) keyType = keyMetacom;
	else return false;
	DEBUG(F("Type: ")); DEBUGLN(keyType);
	for (byte i = 0;;) {
		if (addr[i] != keyID[i]) return false;
		DEBUGHEX(addr[i]);
		if (++i < 8)DEBUG(':'); else break;
		//keyID[i] = addr[i];  // копируем прочтенный код в ReadID
	}
	return true;
}

void card_number_conv(byte buf[8]) {
	rfidData[4] = (0b01111000 & buf[1]) << 1 | (0b11 & buf[1]) << 2 | buf[2] >> 6;
	rfidData[3] = (0b00011110 & buf[2]) << 3 | buf[3] >> 4;
	rfidData[2] = buf[3] << 5 | (128 & buf[4]) >> 3 | (0b00111100 & buf[4]) >> 2;
	rfidData[1] = buf[4] << 7 | (0b11100000 & buf[5]) >> 1 | 0xF & buf[5];
	rfidData[0] = (0b01111000 & buf[6]) << 1 | (0b11 & buf[6]) << 2 | buf[7] >> 6;
}

//**********EM-Marine***************************
bool vertParityCheck(byte* buf) {  // проверка четности столбцов с данными
	if (1 & buf[7]) return false; byte k;
	k = 1 & buf[1] >> 6 + 1 & buf[1] >> 1 + 1 & buf[2] >> 4 + 1 & buf[3] >> 7 + 1 & buf[3] >> 2 + 1 & buf[4] >> 5 + 1 & buf[4] + 1 & buf[5] >> 3 + 1 & buf[6] >> 6 + 1 & buf[6] >> 1 + 1 & buf[7] >> 4;
	if (k & 1) return false;
	k = 1 & buf[1] >> 5 + 1 & buf[1] + 1 & buf[2] >> 3 + 1 & buf[3] >> 6 + 1 & buf[3] >> 1 + 1 & buf[4] >> 4 + 1 & buf[5] >> 7 + 1 & buf[5] >> 2 + 1 & buf[6] >> 5 + 1 & buf[6] + 1 & buf[7] >> 3;
	if (k & 1) return false;
	k = 1 & buf[1] >> 4 + 1 & buf[2] >> 7 + 1 & buf[2] >> 2 + 1 & buf[3] >> 5 + 1 & buf[3] + 1 & buf[4] >> 3 + 1 & buf[5] >> 6 + 1 & buf[5] >> 1 + 1 & buf[6] >> 4 + 1 & buf[7] >> 7 + 1 & buf[7] >> 2;
	if (k & 1) return false;
	k = 1 & buf[1] >> 3 + 1 & buf[2] >> 6 + 1 & buf[2] >> 1 + 1 & buf[3] >> 4 + 1 & buf[4] >> 7 + 1 & buf[4] >> 2 + 1 & buf[5] >> 5 + 1 & buf[5] + 1 & buf[6] >> 3 + 1 & buf[7] >> 6 + 1 & buf[7] >> 1;
	if (k & 1) return false;
	return true;
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
	return 2;  //таймаут, компаратор не сменил состояние
}

bool readEM_Marie(byte* buf) {
	uint32_t tEnd = millis() + 50;
	byte res, bit, bitmask, ones, BYTE = 0;
again:
	for (bit = 0, bitmask = 128, ones = 0; bit < 64; bit++) {  // читаем 64 bit
		res = ttAComp();
		if (res == 2) return false;				//timeout
		if ((bit < 9) && (res == 0)) {			// если не находим 9 стартовых единиц - начинаем сначала
			if (millis() > tEnd) return false;	//ti = 2;//break;//timeout
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
	TCCR2A = (_BV(COM2A0) | _BV(COM2B1) | _BV(WGM21) | _BV(WGM20)); //Toggle on Compare Match on COM2A (pin 11) и счет таймера2 до OCR2A
	TCCR2B = _BV(WGM22) | _BV(CS20);								// mode 7: Fast PWM //divider 1 (no prescaling)
	OCR2A = 63;														// 63 тактов на период. Частота на COM2A (pin 11) 16000/64/2 = 125 кГц, Скважнось COM2A в этом режиме всегда 50%
	OCR2B = 31;														// Скважность COM2B 32/64 = 50%  Частота на COM2A (pin 3) 16000/64 = 250 кГц
	// включаем компаратор
	ADCSRB &= ~_BV(ACME);  // отключаем мультиплексор AC
	ACSR &= ~_BV(ACBG);    // отключаем от входа Ain0 1.1V
}

bool searchEM_Marine(const bool copyKey = true) {
	bool gr = digitalRead(G_Led); digitalWrite(G_Led, !gr);
	rfidACsetOn();  // включаем генератор 125кГц и компаратор
	delay(6);       //13 мс длятся переходные прцессы детектора
	if (!readEM_Marie(addr)) {
		if (!copyKey) TCCR2A &= 0b00111111;  //Оключить ШИМ COM2A (pin 11)
		digitalWrite(G_Led, gr);
		return false;
	}
	keyType = keyEM_Marine;
	if (copyKey) {
		for (byte i = 0;;) {
			keyID[i] = addr[i];
			DEBUGHEX(addr[i]);
			if (++i < 8) DEBUG(':'); else break;
		}
	}
#ifdef  DEBUG_ENABLE
	card_number_conv(addr);
	DEBUG(F(" ( ID ")); DEBUG(rfidData[4]); DEBUG(F(" data "));
	DEBUG(*(uint32_t*)(rfidData + 1));
	DEBUGLN(F(") Type: EM-Marie "));
#endif //  DEBUG_ENABLE
	if (!copyKey) TCCR2A &= 0b00111111;  //Оключить ШИМ COM2A (pin 11)
	digitalWrite(G_Led, gr);
	return true;
}

void TxBitRfid(bool bit) {
	//if (data & 1) delayMicroseconds(54*8);  // 432 родные значения
	if (bit) delayMicroseconds(49 * 8);  //392 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! стало писать говеные 5577
											  //else delayMicroseconds(24*8);      // 192 родные значения
	else delayMicroseconds(19 * 8);           //152 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	rfidGap(19 * 8);                          //write gap 19
}

void TxByteRfid(byte data) {
	for (byte n_bit = 0; n_bit < 8; n_bit++) {
		TxBitRfid(data & 1);
		data = data >> 1;  // переходим к следующему bit
	}
}

void rfidGap(unsigned int tm) {
	TCCR2A &= 0b00111111;  //Оключить ШИМ COM2A
	delayMicroseconds(tm);
	TCCR2A |= _BV(COM2A0);  // Включить ШИМ COM2A (pin 11)
}

bool T5557_blockRead(byte* buf) {
	byte ti, bitmask = 128, BYTE = 0;//ones = 0;
	ti = ttAComp(2000); //пишем в буфер начиная с 1-го бита 
	if (ti > 0) return false;
	for (byte i = 0; i < 32; i++) {  // читаем стартовый 0 и 32 значащих bit
		ti = ttAComp(2000);
		if (ti == 2) return false;  //timeout// если не находим стартовый 0 - это ошибка
		if (ti) BYTE |= bitmask;
		if ((bitmask >>= 1) == 0) { 
			buf[i >> 3] = BYTE;
			bitmask = 128; 
		}
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
	TxBitRfid(blokAddr & 1);  // адрес блока для записи
	delay(4);                 // ждем пока пишутся данные
	return true;
}

bool write2rfidT5557(byte* buf) {
	bool result;
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
	result = readEM_Marie(addr);
	if (!result) {
		OLED_printError(F("The read key faild"));
	};
	TCCR2A &= 0b00111111;  //Оключить ШИМ COM2A (pin 11)
	for (byte i = 0; i < 8; i++)
		if (addr[i] != keyID[i]) {
			result = false;
			break;
		}
	if (!result) {
		OLED_printError(F("The key copy faild"));
		//Sd_ErrorBeep();
	} else {
		OLED_printError(F("The key has copied"), false);
		//Sd_ReadOK();
		delay(2000);
	}
	//digitalWrite(R_Led, HIGH);
	return result;
}

emRWType getRfidRWtype() {
	uint32_t data32, data33;
	byte buf[4] = { 0 };
	rfidACsetOn();                  // включаем генератор 125кГц и компаратор
	delay(13);                      //13 мс длятся переходные процессы детектора
	rfidGap(30 * 8);                //start gap 30
	sendOpT5557(0b11, 0, 0, 0, 1);  //переходим в режим чтения Vendor ID
	if (!T5557_blockRead(buf)) return rwUnknown;
	((byte*)&data32)[3] = buf[0];
	((byte*)&data32)[2] = buf[1];
	((byte*)&data32)[1] = buf[2];
	((byte*)&data32)[0] = buf[3];
	delay(4);
	rfidGap(20 * 8);                                                  //gap 20
	data33 = 0b00000000000101001000000001000000 | (rfidUsePWD << 4);  //конфиг регистр 0b00000000000101001000000001000000
	sendOpT5557(0b10, 0, 0, data33, 0);                               //передаем конфиг регистр
	delay(4);
	rfidGap(30 * 8);                //start gap 30
	sendOpT5557(0b11, 0, 0, 0, 1);  //переходим в режим чтения Vendor ID
	if (!T5557_blockRead(buf)) return rwUnknown;
	((byte*)&data33)[3] = buf[0];
	((byte*)&data33)[2] = buf[1];
	((byte*)&data33)[1] = buf[2];
	((byte*)&data33)[0] = buf[3];
	sendOpT5557(0b00, 0, 0, 0, 0);  // send Reset
	delay(6);
	if (data32 != data33) return rwUnknown;
	DEBUG(F(" The rfid RW-key is T5557. Vendor ID is "));
	DEBUGHEX(data32); DEBUGLN();
	return T5557;
}

bool write2rfid() {
	byte i = 0;
	if (searchEM_Marine(false)) {
		do {
			if (addr[i] != keyID[i]) break;          // сравниваем код для записи с тем, что уже записано в ключе.
		} while (++i < 8);
		if (i < 8) {  // если коды совпадают, ничего писать не нужно
			//digitalWrite(R_Led, LOW);
			OLED_printError(F("It is the same key"));
			//Sd_ErrorBeep();
			digitalWrite(R_Led, HIGH);
			delay(1000);
			return false;
		}
	}
	emRWType rwType = getRfidRWtype();  // определяем тип T5557 (T5577) или EM4305
	if (rwType != rwUnknown) DEBUG(F("\n Burning rfid ID: "));
	switch (rwType) {
	case T5557:
		return write2rfidT5557(keyID);
		break;  //пишем T5557
	//case EM4305: return write2rfidEM4305(keyID); break;                  //пишем EM4305
	case rwUnknown: break;
	}
	return false;
}

void SendEM_Marine(byte* buf) {
	TCCR2A &= 0b00111111;  // отключаем шим
	digitalWrite(FreqGen, LOW);
	//FF:A9:8A:A4:87:78:98:6A
	delay(20);
	for (byte count = 0, i, bitmask; count < 10; count++) {
		for (i = 0; i < 8; i++) {
			for (bitmask = 128; bitmask; bitmask >>= 1) {
				if ((buf[i] & bitmask)) {
					pinMode(FreqGen, INPUT);
					delayMicroseconds(250);
					pinMode(FreqGen, OUTPUT);
				} else {
					pinMode(FreqGen, OUTPUT);
					delayMicroseconds(250);
					pinMode(FreqGen, INPUT);
				}
				delayMicroseconds(250);
			}
		}
		// delay(1);
	}
}

void SendDallas(byte* buf) {
	/*  iBtnEmul.init(buf);
	//iBtnEmul.waitForRequest(false);
	uint32_t tStart = millis();
	do {
	  if (!iBtnEmul.waitReset(10) ) continue;
	  if (!iBtnEmul.presence() ) continue;
	  if (iBtnEmul.recvAndProcessCmd() ) break;
	} while (millis() < 200 + tStart);  */
}

void loop() {
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
		switch (copierMode) {
		case md_empty: //Sd_ErrorBeep(); break;
		case md_read:
			copierMode = md_write;
			//clearLed();
			//digitalWrite(R_Led, HIGH);
			break;
		case md_write:
			copierMode = md_blueMode;
			//clearLed();
			//digitalWrite(B_Led, HIGH);
			//digitalWrite(Luse_Led, !digitalRead(Luse_Led));
			break;
		case md_blueMode:
			copierMode = md_read;
			//clearLed();
			//digitalWrite(G_Led, HIGH);
			//digitalWrite(Luse_Led, !digitalRead(Luse_Led));
			break;
		}
		OLED_printKey(keyID);
		DEBUG(F("Mode: "));
		DEBUGLN(copierMode);
		//Sd_WriteStep();
	}
	}
	if (EEPROM_key_count > 0) {
		if (BtnUp.shortPress()) {  //при повороте энкодера листаем ключи из eeprom
			if (--EEPROM_key_index < 1) EEPROM_key_index = EEPROM_key_count;
			EEPROM_get_key();
			OLED_printKey(keyID);
			//Sd_WriteStep();
		} else if (BtnDown.shortPress()) {
			if (++EEPROM_key_index > EEPROM_key_count) EEPROM_key_index = 0;
			EEPROM_get_key();
			OLED_printKey(keyID);
			//Sd_WriteStep();
		}
	}
	if ((copierMode != md_empty) && BtnOK.longPress()) {  // Если зажать кнопкку - ключ сохранися в EEPROM
		if (EPPROM_AddKey(keyID)) {
			OLED_printError(F("The key saved"), false);
			//Sd_ReadOK();
			delay(1000);
		} else {//Sd_ErrorBeep();
			OLED_printKey(keyID);
		}
	}
	//else if (millis() - stTimer < 100) return;  //задержка в 100 мс
	//stTimer = millis();
	switch (copierMode) {
	case md_empty: case md_read:
		if (searchKT() || searchEM_Marine() || searchIbutton()) {  // запускаем поиск cyfral, затем поиск EM_Marine, затем поиск dallas
		  //keyID[0] = 0xFF; keyID[1] = 0xA9; keyID[2] =  0x8A; keyID[3] = 0xA4; keyID[4] = 0x87; keyID[5] = 0x78; keyID[6] = 0x98; keyID[7] = 0x6A;//это чей-то рабочий код RFID. тут можно
		  //подменять каждое число. копир, при прикладывании ключа, будет выдавать эти числа и их можно записать в память. для RFID надо вычислять код
		  //keyID[0] = 0xFF; keyID[1] = 0xFB; keyID[2] =  0xDE; keyID[3] = 0xF7; keyID[4] = 0xBD; keyID[5] = 0xEF; keyID[6] = 0x7B; keyID[7] = 0xC0;// RFID FF FF FF FF FF METAKOM_CYFRAL
		  //keyID[0] = 0xFF; keyID[1] = 0x80; keyID[2] =  0x7E; keyID[3] = 0xF7; keyID[4] = 0xBD; keyID[5] = 0xEF; keyID[6] = 0x7B; keyID[7] = 0xC2;// RFID 01 FF FF FF FF  CYFRAL
		  //keyID[0] = 0xFF; keyID[1] = 0xFB; keyID[2] =  0xDE; keyID[3] = 0xF7; keyID[4] = 0xBD; keyID[5] = 0xEF; keyID[6] = 0x00; keyID[7] = 0x62;// RFID FF FF FF FF 01  CYFRAL ПЕРЕВЕРТЫШ
		  //keyID[0] = 0xFF; keyID[1] = 0x99; keyID[2] =  0x8A; keyID[3] = 0xA0; keyID[4] = 0xC6; keyID[5] = 0x90; keyID[6] = 0x5F; keyID[7] = 0xB6;// RFID 36 5A 11 40 BE VIZIT old_code
		  //keyID[0] = 0xFF; keyID[1] = 0xDF; keyID[2] =  0xA9; keyID[3] = 0x00; keyID[4] = 0xC6; keyID[5] = 0xAA; keyID[6] = 0x19; keyID[7] = 0x96;// RFID BE 40 11 5A 36 VIZIT ПЕРЕВЕРТЫШ
		  //keyID[0] = 0xFF; keyID[1] = 0xA9; keyID[2] =  0x8A; keyID[3] = 0xA0; keyID[4] = 0xC6; keyID[5] = 0x90; keyID[6] = 0x5F; keyID[7] = 0xBA;// RFID 56 5A 11 40 BE VIZIT
		  //keyID[0] = 0xFF; keyID[1] = 0xDF; keyID[2] =  0xA9; keyID[3] = 0x00; keyID[4] = 0xC6; keyID[5] = 0xAA; keyID[6] = 0x29; keyID[7] = 0x9A;// RFID BE 40 11 5A 56 VIZIT ПЕРЕВЕРТЫШ
		  //keyID[0] = 0xFF; keyID[1] = 0x80; keyID[2] =  0x00; keyID[3] = 0x00; keyID[4] = 0x00; keyID[5] = 0x00; keyID[6] = 0x00; keyID[7] = 0x00;// RFID 00 00 00 00 00 ELtis
		  //keyID[0] = 0x01; keyID[1] = 0xBE; keyID[2] =  0x40; keyID[3] = 0x11; keyID[4] = 0x5A; keyID[5] = 0x36; keyID[6] = 0x00; keyID[7] = 0xE1;//01 BE 40 11 5A 36 00 E1 для Vizit
		  //keyID[0] = 0x01; keyID[1] = 0xBE; keyID[2] =  0x40; keyID[3] = 0x11; keyID[4] = 0x5A; keyID[5] = 0x56; keyID[6] = 0x00; keyID[7] = 0xBB;//01 BE 40 11 5A 56 00 BB для Vizit
		  //keyID[0] = 0x01; keyID[1] = 0xFF; keyID[2] =  0xFF; keyID[3] = 0xFF; keyID[4] = 0xFF; keyID[5] = 0xFF; keyID[6] = 0xFF; keyID[7] = 0x2F;//01 FF FF FF FF FF FF 2F метако,цифра,ВИЗИ
		  //keyID[0] = 0x01; keyID[1] = 0xFF; keyID[2] =  0xFF; keyID[3] = 0xFF; keyID[4] = 0xFF; keyID[5] = 0x00; keyID[6] = 0x00; keyID[7] = 0x9B;//01 FF FF FF FF 00 00 9B метаком,ВИЗИТ
		  //keyID[0] = 0x01; keyID[1] = 0xFF; keyID[2] =  0xFF; keyID[3] = 0x01; keyID[4] = 0x00; keyID[5] = 0x00; keyID[6] = 0x00; keyID[7] = 0x2D;//01 FF FF 01 00 00 00 2D цифрал
		  //keyID[0] = 0x01; keyID[1] = 0x00; keyID[2] =  0x00; keyID[3] = 0x00; keyID[4] = 0x00; keyID[5] = 0x00; keyID[6] = 0x00; keyID[7] = 0x3D;//01 00 00 00 00 00 00 3D цифрал +
		  //keyID[0] = 0x01; keyID[1] = 0xA9; keyID[2] =  0xE4; keyID[3] = 0x3C; keyID[4] = 0x09; keyID[5] = 0x00; keyID[6] = 0x00; keyID[7] = 0xE6;//01 A9 E4 3C 09 00 00 E6 Элтис до 90%
		  //memcpy(keyID, rom, 8);
			//Sd_ReadOK();
			copierMode = md_read;
			digitalWrite(G_Led, HIGH);
			OLED_printKey(keyID, true);
		}break;
	case md_write:
		if (keyType == keyEM_Marine) write2rfid();
		else write2iBtn();
		break;
	case md_blueMode:
		SendEM_Marine(keyID);
		break;
	}  //end switch
}

//***************** звуки****************
void Sd_ReadOK() {  // звук ОК
	for (int i = 400; i < 6000; i = i * 1.5) {
		tone(speakerPin, i);
		delay(20);
	}
	noTone(speakerPin);
}

void Sd_WriteStep() {  // звук "очередной шаг"
	for (int i = 2500; i < 6000; i = i * 1.5) {
		tone(speakerPin, i);
		delay(10);
	}
	noTone(speakerPin);
}

void Sd_ErrorBeep() {  // звук "ERROR"
	for (int j = 0; j < 3; j++) {
		for (int i = 1000; i < 2000; i = i * 1.1) {
			tone(speakerPin, i);
			delay(10);
		}
		delay(50);
		/* for (int i=1000; i>500; i=i*1.9) { tone(speakerPin, i); delay(10); }
		delay(50);*/
	}
	noTone(speakerPin);
}

void Sd_StartOK() {  // звук "Успешное включение"
	tone(speakerPin, NOTE_A7);
	delay(100);
	/* tone(speakerPin, NOTE_G7); delay(100);
	tone(speakerPin, NOTE_E7); delay(100);
	tone(speakerPin, NOTE_C7); delay(100);
	tone(speakerPin, NOTE_D7); delay(100);
	tone(speakerPin, NOTE_B7); delay(100);
	tone(speakerPin, NOTE_F7); delay(100);
	tone(speakerPin, NOTE_C7); delay(100);*/
	noTone(speakerPin);
}
