# <center>stm32-boot</center>
## 简介
    stm32在线升级程序，bootloader程序通过仿真器烧录到flash中stm32在线升级程序;  
	bootloader程序通过仿真器烧录到flash中,从uart1中接收命令和app的bin升级。
## 测试
    在stm32RBT6 flash:128KB ram:20KB上测试使用，其他型号请自行修改bootloader.ioc.

##流程说明： 
```
graph TB
    A1((stm32上电)) -->  B0{等待bootcmd命令}

    B0 --> |接收到cmd命令| D1[进入bootcmd模式] 
    B0 --> |3S内没有收到cmd命令| B1{校验app代码crc通过?}

    B1 --> |Y| C1[运行app程序]
    B1 --> |N| C2[进入升级流程]
    
    C2 --> D1

    D1 --> |FE A5 01| E1[建立连接] 
    D1 --> |FE A5 02| E2[擦除FLASH] 
    D1 --> |FE A5 04| E4[开始写入flash] 
    D1 --> |FE A5 05| E5[写入crc] 
    D1 --> |FE A5 F2| E6[单片机重启] 
    D1 --> |FE A5 F3| E7[强制跳转到app] 
    D1 --> |FE A5 F4| E8[升级测试] 

    E1--> F1[ret: FE A5 01 03 FF 03 00 01]
    E2--> F2[延时1s]
    E4--> F4[连续写入,每次写入小于256字节,间隔10ms]
    F4 --> |FE A5 F1| G1[结束编程] 
```

##内存分配：

use | start addr | end addr | size
---|---|---|---
total| 0x08000000 | 0x0801FFFF |0x00020000
bootloader | 0x08000000 | 0x08003FFF |0x00004000
app| 0x08004000 | 0x08013FFF |0x00010000
free| 0x08014000 | 0x0801FBFE |0x0000BBFE
Private data| 0x801F000 | 0x0801FFFF |0x00001000

## 演示
    如下图：1、打开串口，给已经烧入bootloader的单片机上电且复位。
           2、按照图中1-7顺序执行。
           3、注意第四步，加载app测试程序的bin文件，在发送设置中设置每发送256字节延迟100ms，发送文件。
           4、重启后单片机三秒后执行app程序，如果没有跳转到app，请检查crc。
![演示](docs/test.png)

## 测试
   在/test/目录下由两个bin文件可以测试使用，通过bootloader更新bin后在串口会看到不同的打印效果。
