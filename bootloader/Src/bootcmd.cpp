#include "bootcmd.h"
#include "usart.h"
#include "crc.h"
#include "debug.h"
#include "bootloader.h"

#include <string.h>
#include <stdlib.h>

#define  APP_BASE_ADDRESS            0x8004000   //!< 应用程序起始地址
#define  PRIVATE_DATA_BASE_ADDRESS   0x801F000  //!< 私有数据起始地址
#define  APP_OFFSET_MAX              0x10000     //!< 应用程序地址空间最大偏移量
#define  PRIVATE_DATA_OFFSET_MAX     0x1000       //!< 私有数据地址空间最大偏移量
#define  UART_BUFF_SIZE              2048        //!< 串口缓冲区大小，此处需要根据上位机发送速度适当调整。


#define  PRIVATE_APPSIZE_OFFSET      0           //!< 私有数据 4字节APP固件长度 存储偏移地址
#define  PRIVATE_CRC_OFFSET          4           //!< 私有数据 32位CRC          存储偏移地址


uint8_t g_uartRxBuff[UART_BUFF_SIZE] = {0};       //! 串口接收缓冲区
uint8_t g_bootBuff[UART_BUFF_SIZE]   = {0};       //! bootData缓冲区

volatile uint32_t  g_uart1RxFlag  = 0;            //! uart 数据更新标志，在HAL_UART_RxCpltCallback中增加，由用户访问g_bootBuff后减小。
volatile uint32_t  g_bootBuffSize = 0;            //! g_bootBuff从uart中更新的数据量。


static void RunBootCmd(uint8_t cmd);
static void RunCmdUpdateApp();
static void RunCmdJumpToApp();

static void RunCmdConnection();
static void RunCmdEraseFlash();
static void RunCmdProgramFlash();
static void RunCmdWriteCrc();
static void RunCmdRestart();	
static bool WritePrivateData(char *pBuf, int size, uint32_t offset);
static bool CrcVerify();
/**
  * @brief  Rx Transfer completed callbacks.
  * @param  huart: pointer to a UART_HandleTypeDef structure that contains
  *                the configuration information for the specified UART module.
  * @retval None
  */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  /* Prevent unused argument(s) compilation warning */
  UNUSED(huart);
	static volatile uint32_t lastPosit = 0; 
	
	int curPosit  = huart->RxXferSize - __HAL_DMA_GET_COUNTER(huart->hdmarx);
	//DEBUG("lastPosit: %d curPosit:%d ",lastPosit, curPosit);

	/*! bootbuf正在使用 或者 DMA缓存位置没有变化时直接退出*/
	if( (g_uart1RxFlag > 0) || (curPosit == lastPosit) )
	{
		//ERROR("count: %d",  __HAL_DMA_GET_COUNTER(huart->hdmarx));
		return;
	}
	
	/*! 清空bootbuf */
	g_bootBuffSize = 0;
	memset(g_bootBuff,0x00,sizeof(g_bootBuff)); //清空数组
	
	
	/*! 从DMAbuf中转移数据到bootbuf中 */
	if( curPosit > lastPosit)
	{
		int cpySize1 = curPosit - lastPosit;
		
		memcpy(g_bootBuff, huart->pRxBuffPtr + lastPosit, cpySize1);
		g_bootBuffSize = cpySize1;
		
	}
	else if( curPosit < lastPosit)
	{
		int cpySize1 = huart->RxXferSize - lastPosit;
		int cpySize2 = curPosit;
		
		memcpy(g_bootBuff,          huart->pRxBuffPtr + lastPosit, cpySize1);
		memcpy(g_bootBuff+cpySize1, huart->pRxBuffPtr,              cpySize2);
		g_bootBuffSize = cpySize1 + cpySize2;
		
	}
	lastPosit = curPosit;
  g_uart1RxFlag++;
	
	//DEBUG("--------- %d \n", g_uart1RxFlag);	
  //DEBUG("%s\n", g_bootBuff);	
}

/** @brief     boot主循环。
  */
void DoLoop()
{	
#if 0
	HAL_UART_Receive_DMA(&huart1, g_uartRxBuff, UART_BUFF_SIZE);
	
	do
	{
		if( (g_uart1RxFlag > 0))
		{
			g_uart1RxFlag--;
			
			if((g_bootBuffSize >= 3))
			{
				if( (g_bootBuff[0] == 0xFE) && (g_bootBuff[1] == 0xA5) )
				{
					RunBootCmd(g_bootBuff[2]);
				}
			}
		}
		
		HAL_GPIO_TogglePin(GPIOD, led1_Pin);
		HAL_Delay(100);
	}while(1);
#endif
	HAL_UART_Receive_DMA(&huart1, g_uartRxBuff, UART_BUFF_SIZE);
	
	bool bBootCmdLoop = false;
	while(1)
	{
		int  timeOut100ms = 30;
		while( (timeOut100ms--) > 0)
		{
			if( (g_uart1RxFlag > 0))
			{
				g_uart1RxFlag--;

				if((g_bootBuffSize >= 3))
				{
					bBootCmdLoop = true;

					if( (g_bootBuff[0] == 0xFE) && (g_bootBuff[1] == 0xA5) )
					{
						RunBootCmd(g_bootBuff[2]);
					}
				}
			}

			HAL_GPIO_TogglePin(GPIOD, led1_Pin);
			HAL_Delay(100);
		}

		if( (!bBootCmdLoop) && CrcVerify())
		{
			RunCmdJumpToApp();
		}
	}
}

/** @brief     boot cmd命令执行函数
  * @param[in] cmd : 命令  
  */
static void RunBootCmd(uint8_t cmd)
{
		switch(cmd)
		{
			case 0x01:
				RunCmdConnection();
				break;
			case 0x02:
				RunCmdEraseFlash();
				break;
			case 0x04:
				RunCmdProgramFlash();
				break;
			case 0x05:
				RunCmdWriteCrc();
			break;
			case 0xF2:
				RunCmdRestart();
				break;
			case 0xF3:
				RunCmdJumpToApp();
				break;
			default :
				break;
		}
}

static void RunCmdConnection()
{
	uint8_t temp[] = {0XFE, 0XA5, 0X01, 0X03, 0XFF, 0X03, 0X00, 0X01};
	HAL_UART_Transmit(&huart1, temp, sizeof(temp)/sizeof(temp[0]), 0xFFFF);
	DEBUG("---------------Connection ---------");
}

static void RunCmdEraseFlash()
{
	/*! step 1  Erase Flash*/
	FlashErase(APP_BASE_ADDRESS, APP_BASE_ADDRESS + APP_OFFSET_MAX);	
	DEBUG("--------------- Erase Flash ---------");
}

static void RunCmdProgramFlash()
{
	uint32_t startAddr = APP_BASE_ADDRESS;

	DEBUG("--------------- write Flash---------");
	g_uart1RxFlag = 0;
	uint32_t  count                 = 0;
	const int alignWidth            = 8;
	uint8_t   alignBuff[alignWidth] = {0};
	int       alignBuffOffset       = 0;
	
	while(1)
	{
		if(g_uart1RxFlag > 0)
		{
			bool bQuit     = false;
			/*! Exit programming */
			if(g_bootBuff[g_bootBuffSize-3] == 0xFE  &&  
				 g_bootBuff[g_bootBuffSize-2] == 0xA5  &&
			   g_bootBuff[g_bootBuffSize-1] == 0xF1  )
			{
				bQuit = true;
				g_bootBuffSize -= 3;
				DEBUG("----------------g_bootBuffSize:%d ----------------------------\n", g_bootBuffSize);	
			}
			
			int  preSize   =  ((alignWidth - alignBuffOffset) > g_bootBuffSize) ? g_bootBuffSize : (alignWidth - alignBuffOffset);
			int  postSize  =  (g_bootBuffSize - preSize) % alignWidth;
			int  writeSize = 0;
			
			/*! preorder */
			memcpy( alignBuff + alignBuffOffset, g_bootBuff, preSize);
			
			writeSize = alignBuffOffset + preSize;
			FlashWrite( (char *)alignBuff, startAddr + count, writeSize);
			count += writeSize;
			
			/*! inorder */
			writeSize = g_bootBuffSize - preSize - postSize;
			FlashWrite( ((char *)g_bootBuff) + preSize , startAddr + count, writeSize);
			count += writeSize;
			
			/*! postorder*/
			memcpy( alignBuff, g_bootBuff + (g_bootBuffSize - postSize), postSize);
			alignBuffOffset = postSize;
			
			if(bQuit)
			{
				FlashWrite( (char *)alignBuff , startAddr + count, alignBuffOffset);
				count += alignBuffOffset;
				DEBUG("----------------write flash count:%d ----------------------------\n", count);	
				break;	
			}
			DEBUG("----------------count:%d  buf size:%d  flag:%d  alignBuffOffset:%d \n", count, g_bootBuffSize, g_uart1RxFlag, alignBuffOffset);	
			
			g_uart1RxFlag--;
		}	
	}
	
  /*! 记录app固件长度，写在私有数据区域*/
	WritePrivateData( (char *)(&count), sizeof(count), PRIVATE_APPSIZE_OFFSET);
}

static void RunCmdWriteCrc()
{
	do{
		if( g_uart1RxFlag > 0 )
		{
			g_uart1RxFlag--;
			if(g_bootBuffSize >= 4)
			{
				uint32_t crcValue = 0;
				memcpy((uint8_t*)(&crcValue), g_bootBuff, 4);
				WritePrivateData( (char *)(&crcValue), sizeof(crcValue), PRIVATE_CRC_OFFSET);
				return;
			}
		}
	}while(1);
}

static void RunCmdRestart()
{
	HAL_NVIC_SystemReset();
}


static bool WritePrivateData(char *pBuf, int size, uint32_t offset)
{
	if( (offset + size) > PRIVATE_DATA_OFFSET_MAX)
	{
		return false;
	}
	
	bool bRet = true;
	
  char *pTempBuff = (char *)malloc(PRIVATE_DATA_OFFSET_MAX);
	assert(pTempBuff != NULL);
	
	pTempBuff = (char*)memset(pTempBuff, 0, PRIVATE_DATA_OFFSET_MAX);
	assert(pTempBuff != NULL);
	
	/*! step 1 */
	FlashRead( pTempBuff, PRIVATE_DATA_BASE_ADDRESS, PRIVATE_DATA_OFFSET_MAX);
	
	/*! step 2 */
	FlashErase(PRIVATE_DATA_BASE_ADDRESS, PRIVATE_DATA_BASE_ADDRESS + PRIVATE_DATA_OFFSET_MAX);	

	/*! step 3 */
	memcpy(pTempBuff + offset, pBuf, size);
	
	/*! step 4 */
	if (PRIVATE_DATA_OFFSET_MAX != FlashWrite( pTempBuff, PRIVATE_DATA_BASE_ADDRESS, PRIVATE_DATA_OFFSET_MAX) )
	{
		ERROR("Write private data.");
		bRet = false;
	}
	
	free(pTempBuff);
	pTempBuff = NULL;
	
	return bRet;
}

static bool CrcVerify()
{
	uint32_t appLen = 0;
	FlashRead((char*)(&appLen), PRIVATE_DATA_BASE_ADDRESS + PRIVATE_APPSIZE_OFFSET, 4 );
	DEBUG("applen: %d", appLen);
	
	uint32_t appCrc = 0;
	FlashRead((char*)(&appCrc), PRIVATE_DATA_BASE_ADDRESS + PRIVATE_CRC_OFFSET, 4 );
	DEBUG("appcrc: 0x%04x", appCrc);
	
	uint32_t crcValue1 = 0;
	if(appLen <= APP_OFFSET_MAX)
	{
		crcValue1 = HAL_CRC_Calculate(&hcrc, (uint32_t *)APP_BASE_ADDRESS, appLen/4);
		DEBUG("crcvalue: 0x%04x ", crcValue1);
		
		/*
		uint32_t value = 0x101010;
		crcValue1 = HAL_CRC_Calculate(&hcrc, &value, 1);
		DEBUG("crcvalue: 0x%04x ", crcValue1);
		
		uint32_t ttt[4] = {0};
		memcpy(ttt, (uint8_t*)&value, 4);
		crcValue1 = HAL_CRC_Calculate(&hcrc, ttt, 1);
		DEBUG("crcvalue: 0x%04x ", crcValue1);
		*/
	}
	else
	{	
		return false;
	}
	

	return (appCrc == crcValue1)? true : false;
}

/** @brief     跳转到app执行 
  */
static void RunCmdJumpToApp()
{
#if 1	
	const int readSize = 2048;
	char *pTempBuff = (char *)malloc(readSize);
	assert(pTempBuff != NULL);
	
	pTempBuff =(char*)memset(pTempBuff, 0, readSize);
	assert(pTempBuff != NULL);
	
	FlashRead((char*)pTempBuff, APP_BASE_ADDRESS, readSize);
	
	free(pTempBuff);
	pTempBuff = NULL;
#endif	
	
	DEBUG("---------------Jump To App---------");
	__set_PRIMASK(1);//关总中断,在app中重新开启。
	JumpToApp(APP_BASE_ADDRESS);
}

/** @brief     从过串口升级函数
  * @note      从串口结束app程序的二进制文件， 已0x55 0xAA为结束标志，文件传输完成后自动跳转到app执行 
  */
static void RunCmdUpdateApp()
{
	uint32_t startAddr = APP_BASE_ADDRESS                   /*(g_bootBuff[2]<<24) + (g_bootBuff[3]<<16) + (g_bootBuff[4]<<8) + g_bootBuff[5]*/;
	uint32_t endAddr   = APP_BASE_ADDRESS + APP_OFFSET_MAX  /*(g_bootBuff[6]<<24) + (g_bootBuff[7]<<16) + (g_bootBuff[8]<<8) + g_bootBuff[9]*/;
	
	/*! step 1  Erase Flash*/
	DEBUG("---------------step 1  Erase Flash---------");
	FlashErase(startAddr, endAddr);

	/*! step 2 write Flash*/
	DEBUG("---------------step 2 write Flash---------");
	uint32_t count = 0;
	g_uart1RxFlag = 0;
	const int alignWidth            = 8;
	uint8_t   alignBuff[alignWidth] = {0};
	int       alignBuffOffset       = 0;
	
	while(1)
	{
		if(g_uart1RxFlag > 0)
		{
			bool bQuit     = false;
			/*! Exit programming */
			if(g_bootBuff[g_bootBuffSize-3] == 0xFE  &&  
				 g_bootBuff[g_bootBuffSize-2] == 0xA5  &&
			   g_bootBuff[g_bootBuffSize-1] == 0xF1  )
			{
				bQuit = true;
				g_bootBuffSize -= 3;
				DEBUG("----------------g_bootBuffSize:%d ----------------------------\n", g_bootBuffSize);	
			}
			
			int  preSize   =  ((alignWidth - alignBuffOffset) > g_bootBuffSize) ? g_bootBuffSize : (alignWidth - alignBuffOffset);
			int  postSize  =  (g_bootBuffSize - preSize) % alignWidth;
			int  writeSize = 0;
			
			/*! preorder */
			memcpy( alignBuff + alignBuffOffset, g_bootBuff, preSize);
			
			writeSize = alignBuffOffset + preSize;
			FlashWrite( (char *)alignBuff, startAddr + count, writeSize);
			count += writeSize;
			
			/*! inorder */
			writeSize = g_bootBuffSize - preSize - postSize;
			FlashWrite( ((char *)g_bootBuff) + preSize , startAddr + count, writeSize);
			count += writeSize;
			
			/*! postorder*/
			memcpy( alignBuff, g_bootBuff + (g_bootBuffSize - postSize), postSize);
			alignBuffOffset = postSize;
			
			if(bQuit)
			{
				FlashWrite( (char *)alignBuff , startAddr + count, alignBuffOffset);
				count += alignBuffOffset;
				DEBUG("----------------write flash count:%d ----------------------------\n", count);	
				break;	
			}
			DEBUG("----------------count:%d  buf size:%d  flag:%d  alignBuffOffset:%d \n", count, g_bootBuffSize, g_uart1RxFlag, alignBuffOffset);	
		  g_uart1RxFlag--;
		}	
	}
	
	/*! step 3 chaek flash*/
	/*！ GOTO CRC*/
	DEBUG("---------------step 3 chaek flash---------");
	uint8_t temp[256] = {0};
	FlashRead((char*)temp, APP_BASE_ADDRESS, 256);
	uint32_t crcValue = HAL_CRC_Accumulate(&hcrc, (uint32_t *)temp, 256/4);
	DEBUG("crc = %d", crcValue);
	
	/*! step 4 Jump To App*/
	DEBUG("---------------step 4 Jump To App---------");
	__set_PRIMASK(1);//关总中断,在app中重新开启。
	JumpToApp(APP_BASE_ADDRESS);
	
}
