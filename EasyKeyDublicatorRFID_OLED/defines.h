#pragma once
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
#define rfidUsePWD 0    // ���� ���������� ������ ��� ���������
#define rfidPWD 123456  // ������ ��� �����
#define rfidBitRate 2   // �������� ������ � rfid � kbps
#define COMPARATOR (ACSR & _BV(ACO))
#define TIMER2MASK (_BV(COM2A0) | _BV(COM2B1) | _BV(WGM21) | _BV(WGM20))
//pins
#define iButtonPin PIN_A3   // ����� data ibutton
//#define iBtnEmulPin A1  // ����� ��������� ibutton
#define Luse_Led 13     // ��������� ����
#define R_Led 2         // RGB Led
#define G_Led 3
#define B_Led 4
#define ACpin 6        // ���� Ain0 ����������� ����������� 0.1� ��� EM-Marie
#define speakerPin 12  // ������, �� �� buzzer, �� �� beeper
#define FreqGen 11     // ��������� 125 ���
//#define CLK 8          // s1 ��������
//#define DT 9           // s2 ��������
#define BtnUpPin 8    // ������ �����
#define BtnDownPin 9  // ������ ����
#define BtnOKPin 10      // ������ ������������ ������ ������/������
#define EEPROM_KEY_COUNT (E2END)
#define EEPROM_KEY_INDEX (E2END - 1)

enum key_type : uint8_t {
	keyUnknown,
	keyDallas,
	keyCyfral,
	keyMetacom,
	keyEM_Marine
};  // ��� ������������� �����

enum myMode : uint8_t {
	md_empty,
	md_read,
	md_write,
	md_blueMode
};  // ����� ������ ��������������

enum emRWType : uint8_t {
	Unknown,
	TM2004,
	RW1990_1,
	RW1990_2,
	TM01,
	T5557,
	EM4305
};  // ��� ��������

key_type keyType;
myMode Mode = md_empty;