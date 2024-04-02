#ifndef _RGB_LED_H_
#define _RGB_LED_H_

#include <Arduino.h>

#define LED_B 16  // GPIO16/D0
#define LED_G 5   // GPIO05/D1
#define LED_R 4   // GPIO04/D2

// #define COMMON_ANODE

void ledInit();
void ledRed();
void ledGreen();
void ledBlue();
void ledYellow();
void ledMagenta();
void ledCyan();
void ledBlack();
void ledWhite();

#endif
