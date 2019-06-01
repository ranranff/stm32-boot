# <center>stm32-boot</center>
## 简介
    stm32在线升级程序，bootloader程序通过仿真器烧录到flash中stm32在线升级程序;  
	bootloader程序通过仿真器烧录到flash中,从uart1中接收命令和app的bin升级。
## 测试
    在stm32RBT6 flash:128KB ram:20KB上测试使用，其他型号请自行修改bootloader.ioc.

##命令说明： 
	命令 | <center>含义</center>
	:-:|:-
	0x55 0x01 | 进入编程模式
	0x55 0x02 | 跳转到app应用程序
	0x55 0xAA | 退出编程模式，校验app代码区域，跳转到app执行
 
## 演示
    如下图：1、打开串口，给已经烧入bootloader的单片机上电且复位。
           2、首先发送命令x55 0x01，进入编程模式。
           3、加载app测试程序的bin文件，在发送设置中设置每发送256字节延迟100ms，发送文件。
           4、文件发送完成后，发送命令 0x55 0xaa退出编程模式并跳转到app程序运行。
![演示](docs/test.png)

## 测试
   在/test/目录下由两个bin文件可以测试使用，通过bootloader更新bin后在串口会看到不同的打印效果。

