#include <Arduino.h>
#include "proj.h"
#include "aes.h"
//#define DEBUG
#ifndef DEBUG
  #define eprintf( fmt, args... )
  #define DBGOUT(...)
#else
  #if DEBUGHW>0
    #define FOO(...) __VA_ARGS__
    #define DBGOUT dbg_string+= FOO
    #if (DEBUGHW==2)
      #define eprintf(fmt, args...) S.printf(fmt, ##args)
    #elif (DEBUGHW==1 || DEBUGHW==3)
      #define eprintf(fmt, args...) {sprintf(dbg,fmt, ##args);dbg_string+=dbg;dbg[0]=0;}
    #endif
  #else
    #define eprintf( fmt, args... )
    #define DBGOUT(...)
  #endif
#endif

#define OFFS_DIF 19

#define OFFS_180 12
#define OFFS_280 19
#define OFFS_381 28
#define OFFS_481 38
#define OFFS_170 44
#define OFFS_270 51
#define OFFS_370 58
#define OFFS_470 66
#define OFFS_11280 74

extern uint8_t key[16];
uint8_t iv[16];           // Initial-Vector
uint8_t buffer[128];
uint8_t buff2[256];
static int bytes=0;
static int expct=0;
#define timeout 10000
static uint32_t ms=0;
char timecode[16];
bool amisNotOnline=true;
int valid;

void amis_decoder() {
  char cs=0;
  int i;
  valid=0;
  for (i=4;i<bytes-2;++i) cs+=buff2[i];
  if (cs==buff2[bytes-2]) {
    valid++;
    iv[0]=buff2[7+4];
    iv[1]=buff2[7+5];
    iv[2]=buff2[7+0];
    iv[3]=buff2[7+1];
    iv[4]=buff2[7+2];
    iv[5]=buff2[7+3];
    iv[6]=buff2[7+6];
    iv[7]=buff2[7+7];
    for (i=8;i<16;++i) iv[i]=buff2[15];
    AES128_CBC_decrypt_buffer(buffer,    buff2+OFFS_DIF+0,  16, key, iv);
    AES128_CBC_decrypt_buffer(buffer+16, buff2+OFFS_DIF+16, 16, 0, 0);
    AES128_CBC_decrypt_buffer(buffer+32, buff2+OFFS_DIF+32, 16, 0, 0);
    AES128_CBC_decrypt_buffer(buffer+48, buff2+OFFS_DIF+48, 16, 0, 0);
    AES128_CBC_decrypt_buffer(buffer+64, buff2+OFFS_DIF+64, 16, 0, 0);
    yield();
    sprintf(timecode,"0x%02x%02x%02x%02x%02x",buffer[8],buffer[7],buffer[6],buffer[5],buffer[4]);
    dow = buffer[6] >> 5;
    if (dow >0 && dow <8) valid++;
    mon = buffer[8] & 0x0f;
    if (mon >0 && mon < 13) valid++;
    year = ((buffer[8] & 0xf0) >> 1) | (buffer[7]>>5);
    if(year >=23 && year < 40) valid++;
    memcpy(&a_result[0],buffer+OFFS_180,4);
    memcpy(&a_result[1],buffer+OFFS_280,4);
    memcpy(&a_result[2],buffer+OFFS_381,4);
    memcpy(&a_result[3],buffer+OFFS_481,4);
    memcpy(&a_result[4],buffer+OFFS_170,4);
    memcpy(&a_result[5],buffer+OFFS_270,4);
    memcpy(&a_result[6],buffer+OFFS_370,4);
    memcpy(&a_result[7],buffer+OFFS_470,4);
    memcpy(&a_result[8],buffer+OFFS_11280,4);
    if (a_result[8]==0) valid++;
    if (valid==5) {
      new_data = true;
      new_data3 = true;
      if (first_frame ==0) first_frame=3;
      if (amisNotOnline) {
        amisNotOnline=false;
        writeEvent("INFO", "amis", "Data synced", "ok");
      }
    }
    DBGOUT("valid "+String(valid)+"\n");
  }
  bytes =0;
  expct = 0;
  ms = millis() + timeout;
  Serial.write(0xe5);
  //DBGOUT((String)__FUNCTION__+"\r\n");
}


void amis_poll() {
  if (ms==0) ms = millis() + timeout;

  size_t cnt = Serial.available();
  if (cnt) {
    if ((cnt + bytes) < sizeof(buff2)) {
      Serial.readBytes(&buff2[bytes],cnt);
      bytes += cnt;
    }
    else {                                                    // Leerlesen
      do {
        bytes = Serial.readBytes(&buff2[0],sizeof(buff2));    // mit Timeout 10ms!
      } while (bytes > 0);
      bytes = 0;
      expct = 0;
      DBGOUT(F("Overflow\r\n"));
    }

    if (bytes >=5 && expct==0) {
      if (memcmp(buff2,"\x10\x40\xF0\x30\x16",5)==0) {
        Serial.write(0xe5);
        bytes = 0;
      }
      else
      if (buff2[0]==0x68 && buff2[3]==0x68){
        expct=buff2[1]+6;
      }
    }
    //DBGOUT((String)bytes+":"+(String)expct+" "+(String)cnt+"\r\n");
    if (expct && bytes >= expct) amis_decoder();
  }

  if (millis() > ms) {
    ms=0;
    bytes = 0;
    expct = 0;
    if (!amisNotOnline) writeEvent("INFO", "amis", "Not synced", "");
    amisNotOnline=true;
    DBGOUT(F("Timeout\r\n"));
  }
  //DBGOUT((String)__FUNCTION__+"\r\n");
  //DBGOUT(".");
}
