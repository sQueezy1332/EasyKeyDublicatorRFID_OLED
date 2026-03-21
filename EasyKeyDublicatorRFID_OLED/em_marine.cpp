#include "defines.h"
#include "em_marine.h"
#define _BV(bit) (1 << (bit))

//typedef uint8_t byte;

extern "C" unsigned long micros();
extern void delay_us(size_t);
extern void delay_ms(size_t);
extern bool op_amp();
extern void rfid_emul_high();
extern void rfid_emul_low();
extern void rfid_pwm_disable();
extern void rfid_pwm_enable();
extern void write_indication();
extern byte Buffer[8];

				/*  READING  */

static byte column_parity(const byte buf[8]) {
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

static byte rfid_recvbit(size_t timeOut = RFID_HALFBIT * 4) {
	const bool firstHalf = op_amp();
	constexpr int timer = RFID_HALFBIT * 1.4;
	uint32_t time = uS;
	do {
		const bool secondHalf = op_amp();
		if (secondHalf != firstHalf) {
			for (time = uS; op_amp() == secondHalf;) {
				if (uS - time > timer) break;  //1.5
			};
			return secondHalf;
		}
	} while (uS - time < timeOut);
	return ERROR_RFID_COMP_TIMEOUT;  //op_amp dont changed state
}

int readEM_Marine(byte(&buf)[8]) {
	byte result;
again:
	for (int i = 0; i < 9; i++) { //9 ones preambula
		byte bit = rfid_recvbit();
		if (bit > 1) return bit;
		if (bit == 0) {
			//bit = ERROR_RFID_HEADER; 
			goto again;
		}
	}
	for (int i = 5; i != 0; --i) {	//10 nibbles of data and 10 parity row bit
		result = 0;
		for (byte nibble = 0, par = 0, bitmask = 128; nibble < 10; nibble++) {
			byte bit = rfid_recvbit();
			if (bit) {
				if (bit > 1) return bit;
				par ^= 1;
			}
			//if (nibble % 5 == 4) {
			if ((nibble == 5 - 1) || (nibble == 10 - 1)) {
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
	result = 0;
	for (byte bitmask = 0b10000; bitmask != 1 /*skip stop bit*/; bitmask >>= 1) {
		byte bit = rfid_recvbit();
		if (bit) {
			if (bit > 1) return bit;
			result |= bitmask;
		}
	}
	//if ((result & 1) != 0) return ERROR_RFID_STOP_BIT;
	byte _par = column_parity(buf);
	if (result != (_par << 1)) { //also stop bit check
		//bit = ERROR_RFID_PARITY_COL; 
		goto again;
		result <<= 3; result |= _par; //readed parity | calc parity
		buf[0] = result; return NOERROR;
	}
	buf[0] = 0xFF;	//em marine tag
	return NOERROR;
}

int keyCompare(const byte key[8]) {
	int ret = readEM_Marine(Buffer/*, 30 * 1000*/);
	if (ret == NOERROR) {
		for (int i = 1; i < 6; i++) {
			if (Buffer[i] != key[i]) return KEY_MISMATCH;
		}
	}
	return ret;
}
				/*  TM5577 WRITING  */

static void T5557_rfidGap(size_t time) {
	rfid_pwm_disable();
	delay_us(time);
	rfid_pwm_enable();
}

static void T5557_sendBit(bool bit) {
	//if (bit) { delay_us(30 * 8); } delay_us(24 * 8);	//54*8 == 240 + 192; 		

	//if (bit) { delay_us(30 * 8); } delay_us(19 * 8);	//49*8 == 240 + 152; //shit 5577 !!!
	//T5557_rfidGap(19 * 8); //152

	if (bit) { delay_us(30 * 8); } delay_us(15 * 8);	//46*8 == 240 + 120;
	T5557_rfidGap(150);
}

static void T5557_sendOp(byte opCode, uint32_t data = 0, byte block = 0, uint32_t pass = 0, byte lockBit = 0) {
	write_indication();
	T5557_rfidGap(32 * 8);
	T5557_sendBit(opCode >> 1 & 1);
	T5557_sendBit(opCode & 1); //page
	if (opCode == 0) return;
	// password
	T5557_sendBit(lockBit);
	if (data) {
		for (uint32_t bitmask = (1ul << 31); bitmask; bitmask >>= 1) {
			T5557_sendBit(data & bitmask);
		}
	}
	for (byte bitmask = 0b100; bitmask; bitmask >>= 1) {
		T5557_sendBit(block & bitmask);
	}
	delay_ms(4 + 6);		//wait for writing
}

int write_T5557(const byte data[8]) {
	EM_Marine_encode(data, Buffer);
	//delay_ms(4);	//if (opCode == 0) return
	//start gap 30	//moved
	T5557_sendOp(0b10, ((uint32_t*)Buffer)[1], 0x1);
	//delay_ms(6); //moved
	T5557_sendOp(0b10, ((uint32_t*)Buffer)[0], 0x2);
	//delay_ms(6); //moved
	//start gap 30 //moved
	T5557_sendOp(0b00);
	return keyCompare(data);
}

static bool T5557_blockRead(uint32_t& buf) {
	byte ti = rfid_recvbit(2000); //read start 0
	if (ti != 0) return false;
	buf = 0;	//write from msb, if not find start 0 - its error
	//for (uint32_t bitmask = 1; bitmask; bitmask <<= 1) {
	for (uint32_t bitmask = 1ul << 31; bitmask; bitmask >>= 1) {
		if (ti = rfid_recvbit(2000)) {
			if (ti > 1) return false;  //timeout
			buf |= bitmask;
		}
	}
	return true;
}
				/*   EM4305 WRITING  */

static void em4305_FirstFieldStop()
{
	rfid_pwm_disable();
	delay_us(6 * 8);
	rfid_pwm_enable();
	delay_us(12 * 8);
	rfid_pwm_disable();
	delay_us(40 * 8);
	rfid_pwm_enable();
	delay_us(17 * 8);
	rfid_pwm_disable();
}

static void FirstFieldStop_4305(void)									//пауза FirstFieldStop для em4305
{
	rfid_pwm_disable();
	delay_us(55 * 8 + 40);
	rfid_pwm_enable();
	delay_us(18 * 8 - 40);
}

static void em4305_sendZero(void)											//отправить ноль для em4305
{
	rfid_pwm_disable();
	delay_us(18 * 8 + 40);
	rfid_pwm_enable();
	delay_us(18 * 8 - 40);
}

static void em4305_sendOne(void)											//отправить единицу для em4305
{
	rfid_pwm_enable();
	delay_us(32 * 8);
}

static void em4305_write_block(const uint8_t* buf) {						//передать карте Em4305 блок данных 32 бита
	uint8_t col_p = 0;													//шлём данные
	for (int i = 3; i >= 0; --i) {
		uint8_t data = buf[i];
		uint8_t row_p = 0;
		col_p ^= data;
		for (uint8_t mask = 128; mask; mask >>= 1) {
			if (data & mask) {
				em4305_sendOne();
				row_p ^= 1;
			}
			else em4305_sendZero();
		}
		if (row_p) em4305_sendOne();					//шлём чётность по строкам
		else em4305_sendZero();
	}
	for (uint8_t mask = 128; mask; mask >>= 1)					//шлём чётность по столбцам
	{
		if (col_p & mask) em4305_sendOne();
		else em4305_sendZero();
	}
	em4305_sendZero();												//шлём 0
	//rfid_pwm_enable();
	delay_ms(10);
}

static void em4305_sendCmd(uint8_t cmd) {
	em4305_FirstFieldStop();
	for (uint8_t mask = 0b10000; mask; mask >>= 1) {
		if (cmd & mask) em4305_sendOne();
		else em4305_sendZero();
	}

}

static void em4305_sendLogin(uint8_t* data) {	 							//передать карте Em4305 логин
	em4305_sendCmd(0b0011);
	em4305_write_block(data);										//шлём данные
}

static void em4305_send_addr(uint8_t addr) {				//записать слово в Em4305
	//HIGHT - поле выключено
	em4305_sendCmd(0b0101);
	//шлём адрес слова (младший бит первый)
	uint8_t par = 0;
	for (uint8_t mask = 1; mask; mask <<= 1) {
		if (addr & mask) {
			em4305_sendOne();
			par ^= 1;
		}
		else em4305_sendZero();
	}
	//дополняющие нули и чётность
	em4305_sendZero();
	em4305_sendZero();
	if (par) em4305_sendOne();
	else em4305_sendZero();
	//шлём данные...
}

void bit_reverse8(uint8_t* const  data) {
	uint8_t temp = *data, res = 0;
	for (uint8_t mLsb = 1, mMsb = 128; mLsb; mLsb <<= 1, mMsb >>= 1) {
		if (temp & mLsb) res |= mMsb;
	}
	*data = res;
}

int write_em4305(const byte data[8]) {
	rfid_pwm_enable();//включаем электромагнитное поле
	delay_ms(10); //20
	//шлём логин, не обязательно
	//Buffer[0] = 0xFF; //password
	//Buffer[1] = 0xFF;
	//Buffer[2] = 0xFF;
	//Buffer[3] = 0xFF;
	//em4305_sendLogin(Buffer);
	//запись конфигурации (манчестер, RF/64, выдача слова 6)
	//constexpr uint32_t config = 0x0001805F;
	*(uint32_t*)Buffer = 0x5F800100;
//#if __has_builtin(__builtin_bitreverse32)
//#pragma message "__builtin_bitreverse32"
	for (uint8_t n = 0; n < 4; n++) {
		bit_reverse8(&Buffer[n]);
	}
	//* (uint32_t*)Buffer = __builtin_bitreverse32(config);
//#endif

	// Buffer[0] = 0x5F;  
	// Buffer[1] = 0x80;
	// Buffer[2] = 0x01;
	// Buffer[3] = 0x00;
	write_indication();
	em4305_send_addr(4);
	em4305_write_block(Buffer);
	write_indication();
	//задаём номер карты
	EM_Marine_encode(data, Buffer);
	//функция em4305_write_word требует прямой порядок бит, а после перекодирования порядок обратный - переставляем
	for (uint8_t n = 0; n < 8; n++) {
		bit_reverse8(&Buffer[n]);
	}
	write_indication();
	//запись ID карты в слова 5 и 6
	em4305_send_addr(5);
	em4305_write_block(&Buffer[4]);
	em4305_send_addr(6);
	em4305_write_block(&Buffer[0]);

	write_indication();
	rfid_pwm_disable();
	delay_ms(10);
	rfid_pwm_enable();
	//delay_ms(20);
	//return rfid_check(data);
	return keyCompare(data);
}
				/*  GET TYPE  */

int getRFIDtype() {
	//0x14 bit rate = FCK/64 // 0x80 modulation = manchester // 0x40 max block = 2
	const uint32_t config = 0x00148040 | (rfidUsePWD << 4);  //config register
	uint32_t& vendor = reinterpret_cast<uint32_t&>(Buffer);
	uint32_t& vendor2 = reinterpret_cast<uint32_t&>(Buffer[4]);          
	//start gap 30
	T5557_sendOp(0b11, 0, 0x1);		//reading Vendor ID mode // 0b1p - page
	//MSB 0xE0 + 0x15 + CID(5bit) + ICR(3 bit) + (40bit)
	if (!T5557_blockRead(vendor)) return ERROR_READ_2;
	//delay_ms(4);
	//T5557_rfidGap(20 * 8)		//gap 20
	T5557_sendOp(0b10, config);	//send config register
	//delay_ms(4);
	//start gap 30
	T5557_sendOp(0b11, 0, 0x1);		//reading Vendor ID mode
	if (!T5557_blockRead(vendor2)) return ERROR_READ_3;
	T5557_sendOp(0b00);  // send Reset
	//delay_ms(6);
	if (vendor == vendor2) {
		T5557_sendOp(0b10, 0, 0x0);
		if (!T5557_blockRead(vendor2)) return ERROR_READ_4;
		return T5557; 
	}
	return UNKNOWN_TYPE;
}

int writeRfid(const byte data[8]) {
	int type = keyCompare(data);
	if (type == NOERROR)
		return KEY_SAME;
	if (type == KEY_MISMATCH) {
		type = getRFIDtype();
		if (type >= UNKNOWN_TYPE) {
			//DEBUG(F("\nVendor ID: ")); DEBUG(*(uint32_t*)Buffer, HEX);
			//DEBUGLN();
			//DEBUG(F("\tVendor2 ID: ")); DEBUGLN(*(uint32_t*)(Buffer+4), HEX);
		}
		switch (type) {
		case T5557:
			return write_T5557(data);
		case EM4305:
			return write_em4305(data);
		}
	}
	return type;
}
				/*  EMULATE  */

void sendEM_Marine(const byte buf[8], size_t count = 10) {
	//byte buffer[8];//FF:A9:8A:A4:87:78:98:6A //0x0000565A91B831
	EM_Marine_encode(buf, Buffer);
	for (; count > 0; --count) {
		for (int i = 7; i >= 0; --i) {
			for (byte bitmask = 128, data = buf[i]; bitmask; bitmask >>= 1) {
				if (data & bitmask) {
					rfid_emul_high();
					delay_us(250);
					rfid_emul_low();	//high to low
				}
				else {
					rfid_emul_low();
					delay_us(250);
					rfid_emul_high();	//low to high
				}
				delay_us(250);
			}
		}
		rfid_emul_high();
	}
}



/*void TxByteRfid(byte data) {
	for (byte bitmask = 0; bitmask; bitmask <<= 1) {
		T5557_sendBit(data & bitmask);
	}
}*/