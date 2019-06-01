#include "bootloader.h"
#include <string.h>
#include <stdio.h>

#include "debug.h"

typedef  void (*pFunction)(void);

/** @brief     执行flash中的指定位置的代码。
  * @param[in] addr : flash地址。
  * @warning   跳转前不能使用未重定向的printf，否则会出现跳到BEAB等位置。 
  */
void JumpToApp(uint32_t addr)
{
	//DEBUG("%04X", addr);
  if ( ((*(__IO uint32_t *)addr) & 0x2FFE0000 ) == 0x20000000)
  {
    /*！ 应用程序起始地址偏移4，为reset中断服务程序地址*/
    uint32_t  jumpAddress       = *(uint32_t*) (addr + 4);
		
		/*！ 将reset中断服务程序地址转换为函数指针*/
    pFunction JumpToApplication = (pFunction) jumpAddress;            
		
    /* Initialize user application's Stack Pointer */
		__set_PSP(*(volatile unsigned int*) addr);
		__set_CONTROL(0);
		__set_MSP(*(volatile unsigned int*) addr);
		
    JumpToApplication();
  }
	else
	{
    ERROR("not find APPlication...");
  }
}


/** @brief     从flash中读取数据。
  * @param[in] pBuf : 数据保存地址
  * @param[in] addr : flash起始地址
  * @param[in] size : 读取数量
  * @return    成功读到pBuf中的数据量。  
  */
uint8_t FlashRead( char *pBuf, uint32_t addr, int size)
{
	memcpy(pBuf, (char*)addr, size);
	
 	
	DEBUG("-----------------FlashRead--------------------");
	for(int i = 0; i< size; i++)
	{
		printf("%02X ", pBuf[i]);
		if( i % 16 == 0)
		{
			printf("\n");
		}
	}
	printf("\n");
/*!*/	
  return size;
}

/** @brief     向flash中写入数据。
  * @param[in] pBuf : 数据地址
  * @param[in] addr : flash起始地址
  * @param[in] size : 写入数量
  * @return    成功写入数量
  * @warning   起始地址必须是16bit对齐，即写入必须按16位地址开始写入。   
  */
uint8_t FlashWrite( char *pBuf, uint32_t addr, int size)
{
	if ( addr%2 != 0)
	{
	 ERROR(" **************************** addr: %04X", addr);
	 return 0;
	}
	 
	const    int      alignWidth = 8;
  const    uint32_t starAddr   = addr;
  volatile int      remainSize = size;
	volatile int      offset     = 0;
	 
	HAL_FLASH_Unlock();
	while(remainSize > 0)
	{
		uint64_t data = 0xFFFFFFFFFFFFFFFF;
		
    int bufCpySize =  (remainSize >= alignWidth)? alignWidth : remainSize; 
		memcpy(&data, pBuf + offset, bufCpySize);

		//DEBUG("starAddr: %04x   data: %04X %04X",starAddr, (uint32_t)(data>>32), (uint32_t)data);
		if(  HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, starAddr + offset, data) != HAL_OK)
		{
			ERROR(" **************************** error: %d", HAL_FLASH_GetError());
			ERROR(" **************************** starAddr: %04X  offset: %d ", starAddr, offset);
			break;
		}

		offset     += bufCpySize;
		remainSize -= bufCpySize;
	}
	HAL_FLASH_Lock();
	
	return offset;
}

/** @brief     擦除flash，擦除完成后flash中为0XFF。
  * @param[in] startAddr : flash起始地址
  * @param[in] endAddr   : flash结束地址
  * @param[in] size : 写入数量
  * @return    status 擦除状态，成功返回HAL_OK
  * @note      擦除最小单位FLASH_PAGE_SIZE，且按FLASH_PAGE_SIZE对齐，未到FLASH_PAGE_SIZE大小的不擦除。    
  */
uint8_t FlashErase(uint32_t startAddr, uint32_t endAddr)
{
		HAL_FLASH_Unlock();

		//初始化FLASH_EraseInitTypeDef
		FLASH_EraseInitTypeDef f;
		f.TypeErase    =  FLASH_TYPEERASE_PAGES;
		f.PageAddress  =  startAddr;
		f.NbPages      = (endAddr - startAddr) / FLASH_PAGE_SIZE;
	
		//设置PageError
		uint32_t           PageError = 0;
		HAL_StatusTypeDef  status = HAL_FLASHEx_Erase(&f, &PageError);
	
	  DEBUG("error:%d  status:%d\n", status, HAL_FLASH_GetError() );
	
		HAL_FLASH_Lock();  

		return status;
}


/*！ -----------------------------------------------------------------------------------------------------------------------------
---------------------------------------------------------------------------------------------------------------------------------*/
/** @brief flash读写测试函数。 
  */
void FlashTest(void)
{
	/*! read flash*/
	char readBuf[256]  = {0};
	char writeBuf[256] = {0};
	DEBUG("-----------------set buf--------------------");
	for(int i = 0; i< 255; i++)
	{
		writeBuf[i] = i;
		printf("%01X ", writeBuf[i]);
	}

	DEBUG("-----------------Read--------------------");
	FlashRead(readBuf, 0x8004000, 100);

	DEBUG("-----------------Erase--------------------");
	FlashErase(0x8004000, 0x800A000);
	FlashRead(readBuf, 0x8004000, 100);
	
	
#if 0	
	uint64_t data = 0x0123456789abcdef;
	
	if(  HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, 0x8004001, data) != HAL_OK)
			{
					ERROR(" **************************** error: %d", HAL_FLASH_GetError());
				  printf("***************************** data: ");
				  char temp[8] = {0};
					memcpy( temp, &data, 8);
					for(int i=0; i<8; i++)
					{
						printf("%02X ", temp[i]);
					}
					printf("*******\n");
					//break;
			}
	  FlashRead(readBuf, 0x8004000, 20);
#else

	DEBUG("\n\n-----------------Write 1--------------------");
	FlashWrite(writeBuf, 0x8004001, 10);
	FlashRead(readBuf, 0x8004000, 100);

	DEBUG("\n\n-----------------Write 2--------------------");
	FlashWrite(writeBuf, 0x8004000, 10);
	FlashRead(readBuf, 0x8004000, 100);


	DEBUG("\n\n-----------------Write 3--------------------");
	FlashWrite(writeBuf, 0x8004000, 10);
	FlashRead(readBuf, 0x8004000, 100);
#endif

	printf("end\n");
}

