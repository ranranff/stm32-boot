#ifndef __BOOTLOADER_H
#define __BOOTLOADER_H

#include <stm32f1xx_hal.h>

#ifdef __cplusplus
extern "C" {
#endif

void    JumpToApp(uint32_t addr);
uint8_t FlashRead( char *pBuf, uint32_t addr, int size);
uint8_t FlashWrite( char *pBuf, uint32_t addr, int size);
uint8_t FlashErase(uint32_t startAddr, uint32_t endAddr);
void    FlashTest(void);

#ifdef __cplusplus
}
#endif


#endif /* bootloader.h */
