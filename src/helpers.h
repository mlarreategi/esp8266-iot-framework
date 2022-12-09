#ifndef helpers_h

//#include <Arduino.h>

#ifdef DEBUG
#include <HardwareSerial.h>
#define _PP(a) Serial.print(a)
#define _PL(a) Serial.println(a)
#else
#define _PP(a)
#define _PL(a)
#endif

#endif
