#pragma once
#include "pitches.h"
void Sd_ReadOK() {  // звук ОК
	for (int i = 400; i < 6000; i *= 1.5) {
		tone(speakerPin, i);
		delay(20);
	}
	noTone(speakerPin);
}

void Sd_WriteStep() {  // звук "очередной шаг"
	for (int i = 2500; i < 6000; i *= 1.5) {
		tone(speakerPin, i);
		delay(10);
	}
	noTone(speakerPin);
}

void Sd_ErrorBeep() {  // звук "ERROR"
	for (byte j = 0; j < 3; j++) {
		for (int i = 1000; i < 2000; i *= 1.1) {
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
	tone(speakerPin, NOTE_A7); delay(100);
	tone(speakerPin, NOTE_G7); delay(100);
	tone(speakerPin, NOTE_E7); delay(100);
	tone(speakerPin, NOTE_C7); delay(100);
	tone(speakerPin, NOTE_D7); delay(100);
	tone(speakerPin, NOTE_B7); delay(100);
	tone(speakerPin, NOTE_F7); delay(100);
	tone(speakerPin, NOTE_C7); delay(100);
	noTone(speakerPin);
}

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