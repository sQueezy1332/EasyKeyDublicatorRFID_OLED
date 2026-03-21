#pragma once
//byte column_parity(const byte buf[8]);
int readEM_Marine(byte(&)[8]);
int keyCompare(const byte[8]);
int writeRfid(const byte [8]);
int write_T5557(const byte[8]);
int write_em4305(const byte [8]);
int getRFIDtype();
void sendEM_Marine(const byte buf[8], size_t count = 10);
