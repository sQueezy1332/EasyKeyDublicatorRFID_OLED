byte readEM_Marine(byte(&buf)[8], bool copykey = false) {
	//bool gr = digitalRead(G_Led); digitalWrite(G_Led, !gr);
	rfidACsetOn();  // включаем генератор 125кГц и компаратор
	auto tEnd = mS + (copykey ? 1000 * 30 : 64);
	byte res, i, bit, bitmask, p, BYTE;
again:
	if (mS > tEnd) return ERROR_RFID_HEADER_TIMEOUT;
	for (bit = 0; bit < 9; bit++) { //9 ones preambula
		res = recvbit_rfid();
		if (res > 1) return ERROR_RFID_TIMEOUT;
		if (res == 0) goto again;
	}
	for (i = 5; i; --i) {	//10 nibbles of data and 10 row parity bit
		for (BYTE = 0, bit = 0, p = 0, bitmask = 128; bit < 10; bit++) {
			res = recvbit_rfid();
			if (res > 1) { return ERROR_RFID_TIMEOUT; }
			if (res == 1) p^=1;
			//if (bit % 5 == 4) {
			if ((bit == 5 - 1) || (bit == 10 - 1)) {
				if (p) { 
					if (mS > tEnd) return ERROR_RFID_PARITY_ROW;
					goto again;
				}
				continue;
			}
			if (res) BYTE |= bitmask;
			bitmask >>= 1;
		}
		buf[i] = BYTE;
	}
	for (bitmask = _BV(5), BYTE = 0; bitmask; bitmask >>= 1) {//column parity and stop bit
		res = recvbit_rfid();
		if (res > 1) return ERROR_RFID_TIMEOUT;
		if (res) BYTE |= bitmask;
	}
	//if ((BYTE & 1) != 0) return ERROR_RFID_STOP_BIT;
	if (BYTE != (columnParity(buf) << 1)) return ERROR_RFID_PARITY_COL;
	buf[0] = 0xFF;	//em marine tag
	if (!copykey)rfid_disable();
	//digitalWrite(G_Led, gr);
	return NOERROR;
}
