#pragma once

#include <stdint.h>
#include <stdbool.h>

void i8249_Configure(uint8_t offsetPic1, uint8_t offsetPic2, bool autoEoi);
void i8249_SendEndOfInterrupt(int irq);
void i8249_Disable();
void i8249_Mask(int irq);
void i8249_Unmask(int irq);
uint16_t i8249_ReadIrqRequestRegister();
uint16_t i8249_ReadInServiceRegister();
void i8249_SetMask(uint16_t Mask);