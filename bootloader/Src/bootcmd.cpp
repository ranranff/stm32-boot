/**
  ******************************************************************************
  * @file    bootcmd.cpp
  * @brief   与boot相关命令
	* @Author  dengbaoan
	* @Email   645603192@qq.com
  * @Version v0.0.1
  * @Date:   2019.06.11
	* @attention
  ******************************************************************************
  */
#include "bootcmd.h"
#include "usart.h"
#include "crc.h"
#include "debug.h"
#include "bootloader.h"

#include <string.h>
#include <stdlib.h>

#define  APP_BASE_ADDRESS            (0x8004000)   //!< 应用程序起始地址
#define  PRIVATE_DATA_BASE_ADDRESS   (0x801F000)  //!< 私有数据起始地址
#define  APP_OFFSET_MAX              (0x10000)     //!< 应用程序地址空间最大偏移量
#define  PRIVATE_DATA_OFFSET_MAX     (0x1000)       //!< 私有数据地址空间最大偏移量
#define  UART_BUFF_SIZE              (2048)        //!< 串口缓冲区大小，此处需要根据上位机发送速度适当调整。


#define  PRIVATE_APPSIZE_OFFSET      (0)           //!< 私有数据 4字节APP固件长度 存储偏移地址
#define  PRIVATE_CRC_OFFSET          (4)           //!< 私有数据 32位CRC          存储偏移地址


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
  * @note      主循环首先在bootcmd模式等待命令输入，
               如果3s内有命令输入,则永远在bootcmd模式。
               否则区验证app的crc，验证通过去执行app程序，crc不通过则待在bootcmd模式。
  */
void DoLoop()
{
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
    case 0xF4:
    {
        if(CrcVerify())
        {
            RunCmdJumpToApp();
        }
    }
    break;
    case 0xF5:
        RunCmdUpdateApp();
        break;
    default :
        break;
    }
}

/** @brief     链接握手函数
  * @note      没有实际作用，仅仅是对cmd作一个特定应答，在升级之前需要确定单片机已经在工作。
  */
static void RunCmdConnection()
{
    DEBUG("---------------Connection ---------");
    uint8_t temp[] = {0XFE, 0XA5, 0X01, 0X03, 0XFF, 0X03, 0X00, 0X01};
    HAL_UART_Transmit(&huart1, temp, sizeof(temp)/sizeof(temp[0]), 0xFFFF);
}

/** @brief     flash擦除函数
  * @note      对app代码区域进行擦除。
  */
static void RunCmdEraseFlash()
{
    DEBUG("--------------- Erase Flash ---------");
    FlashErase(APP_BASE_ADDRESS, APP_BASE_ADDRESS + APP_OFFSET_MAX);
}

/** @brief     对flash编程
  * @note      将串口接收到的数据写入到flash中，直到遇到0xFE 0xA5 0xF1停止。
  * @note      结束编程后，会将本次编程长度写入私有数据区域。 地址：PRIVATE_DATA_BASE_ADDRESS + PRIVATE_APPSIZE_OFFSET。
  * @warning   如果app的bin文件包含 0xFE 0xA5 0xF1，则会导致数据写入不完整。
  */
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
            FlashWrite( ((char *)g_bootBuff) + preSize, startAddr + count, writeSize);
            count += writeSize;

            /*! postorder*/
            memcpy( alignBuff, g_bootBuff + (g_bootBuffSize - postSize), postSize);
            alignBuffOffset = postSize;

            if(bQuit)
            {
                FlashWrite( (char *)alignBuff, startAddr + count, alignBuffOffset);
                count += alignBuffOffset;
                DEBUG("----------------write finish :%d ----------------------------\n", count);
							  HAL_GPIO_TogglePin(GPIOC, led3_Pin);
                break;
            }
            DEBUG("----------------count:%d  buf size:%d  flag:%d  alignBuffOffset:%d \n", count, g_bootBuffSize, g_uart1RxFlag, alignBuffOffset);
						
            HAL_GPIO_TogglePin(GPIOC, led2_Pin);
            g_uart1RxFlag--;
        }
    }

    /*! 记录app固件长度，写在私有数据区域*/
    WritePrivateData( (char *)(&count), sizeof(count), PRIVATE_APPSIZE_OFFSET);
}

/** @brief     写入app代码的32位crc校验码
  * @note      校验码从串口接收，共四字节，在收到4字节前，一直等待。
  */
static void RunCmdWriteCrc()
{
    do
    {
        if( g_uart1RxFlag > 0 )
        {
            g_uart1RxFlag--;
            if(g_bootBuffSize >= 4)
            {
                uint32_t crcValue = 0;
                memcpy((uint8_t*)(&crcValue), g_bootBuff, 4);
                DEBUG("----------------Write crc:%04x ----------------------------\n", crcValue);
                WritePrivateData( (char *)(&crcValue), sizeof(crcValue), PRIVATE_CRC_OFFSET);
                return;
            }
        }
    }
    while(1);
}

/** @brief     单片机软重启
  */
static void RunCmdRestart()
{
    DEBUG("----------------Restart ----------------------------\n");
    HAL_NVIC_SystemReset();
}

/** @brief     写入私有数据
  * @note      私有数据内存分配在flash中。
  * @warning   flash特性导致每次写入任意数量的数据都会对整个私有数据区域重新擦除，
               所以一定要规划好数据写入的offset，避免将其他数据覆盖。
  */
static bool WritePrivateData(char *pBuf, int size, uint32_t offset)
{
    if( (offset + size) > PRIVATE_DATA_OFFSET_MAX)
    {
        ERROR("offset: %d size: %d",offset, size);
        return false;
    }

    bool bRet = true;

    char *pTempBuff = (char *)malloc(PRIVATE_DATA_OFFSET_MAX);
    assert(pTempBuff != NULL);

    pTempBuff = (char*)memset(pTempBuff, 0, PRIVATE_DATA_OFFSET_MAX);
    assert(pTempBuff != NULL);

    /*! step 1 将数据全部读取到buf*/
    FlashRead( pTempBuff, PRIVATE_DATA_BASE_ADDRESS, PRIVATE_DATA_OFFSET_MAX);

    /*! step 2 擦除私有区域*/
    FlashErase(PRIVATE_DATA_BASE_ADDRESS, PRIVATE_DATA_BASE_ADDRESS + PRIVATE_DATA_OFFSET_MAX);

    /*! step 3 修改数据*/
    memcpy(pTempBuff + offset, pBuf, size);

    /*! step 4 将数据一次性写入*/
    if (PRIVATE_DATA_OFFSET_MAX != FlashWrite( pTempBuff, PRIVATE_DATA_BASE_ADDRESS, PRIVATE_DATA_OFFSET_MAX) )
    {
        ERROR("Write private data.");
        bRet = false;
    }

    free(pTempBuff);
    pTempBuff = NULL;

    return bRet;
}

/** @brief     使用stm32的硬件crc校验
  * @note      计算app代码区域的crc，与私有数据中的crc对比
  * @warning   stm32的硬件crc是32位以太网校验，与主流crc有所不同。
  */
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

    /*! 关闭一些已经打开的外设*/
    __HAL_UART_DISABLE_IT(&huart1, UART_IT_IDLE);
    HAL_UART_MspDeInit(&huart1);

    //__set_PRIMASK(1);//关总中断,在app中重新开启。
    __disable_irq();

    JumpToApp(APP_BASE_ADDRESS);
}

/** @brief     从过串口升级函数
  * @note      从串口接收app程序的二进制文件，文件传输完成后自动跳转到app执行，不进行crc校验。
  */
static void RunCmdUpdateApp()
{
    RunCmdEraseFlash();
    RunCmdProgramFlash();
    RunCmdJumpToApp();
}
