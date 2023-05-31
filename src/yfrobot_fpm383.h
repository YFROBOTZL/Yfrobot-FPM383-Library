/******************************************************************************
  yfrobot_fpm383.h
  YFROBOT FPM383 Sensor Library Source File
  Creation Date: 05-31-2023
  @ YFROBOT

  Distributed as-is; no warranty is given.
******************************************************************************/

#ifndef YFROBOTFPM383_H
#define YFROBOTFPM383_H

#include "Arduino.h"
#include <SoftwareSerial.h>

#define RECEIVE_TIMEOUT_VALUE 1000 // Timeout for I2C receive

class YFROBOTFPM383
{
  private:
    
    uint8_t PS_ReceiveBuffer[20];  //串口接收数据的临时缓冲数组

    /********************************************** 指纹模块 指令集 ************************************************/
    //指令/命令包格式：                  包头0xEF01  设备地址4bytes 包标识1byte 包长度2bytes 指令码1byte  参数1......参数N  校验和2bytes
    //验证用获取图像 0x01，验证指纹时，探测手指，探测到后录入指纹图像存于图像缓冲区。返回确认码表示：录入成功、无手指等。
    uint8_t PS_GetImageBuffer[12]   = { 0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x03, 0x01, 0x00, 0x05 };
    // 生成特征 0x02，将图像缓冲区中的原始图像生成指纹特征文件存于模板缓冲区。
    uint8_t PS_GetCharBuffer[13]   = { 0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x04, 0x02, 0x01, 0x00, 0x08 };
    // uint8_t PS_GetChar2Buffer[13]   = { 0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x04, 0x02, 0x02, 0x00, 0x09 };
    // 搜索指纹 0x04，以模板缓冲区中的特征文件搜索整个或部分指纹库。若搜索到，则返回页码。加密等级设置为 0 或 1 情况下支持此功能。
    uint8_t PS_SearchMBBuffer[17]   = { 0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x08, 0x04, 0x01, 0x00, 0x00, 0xFF, 0xFF, 0x02, 0x0C };
    // 删除指纹 0x0C，删除 flash 数据库中指定 ID 号开始的1 个指纹模板。//PageID: bit 10:11，SUM: bit 14:15
    uint8_t PS_DeleteBuffer[16]     = { 0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x07, 0x0C, '\0', '\0', 0x00, 0x01, '\0', '\0' };
    // 清空指纹库 0x0D，删除 flash 数据库中所有指纹模板。
    uint8_t PS_EmptyBuffer[12]      = { 0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x03, 0x0D, 0x00, 0x11 };
    // 取消指令 0x30，取消自动注册模板和自动验证指纹。加密等级设置为 0 或 1 情况下支持此功能。
    uint8_t PS_CancelBuffer[12]     = { 0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x03, 0x30, 0x00, 0x34 };
    // 自动注册 0x31，一站式注册指纹，包含采集指纹、生成特征、组合模板、存储模板等功能。加密等级设置为 0 或 1 情况下支持此功能。
    uint8_t PS_AutoEnrollBuffer[17] = { 0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x08, 0x31, '\0', '\0', 0x04, 0x00, 0x16, '\0', '\0' };
    // 休眠指令 0x33，设置传感器进入休眠模式。
    uint8_t PS_SleepBuffer[12]      = { 0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x03, 0x33, 0x00, 0x37 };
    // LED控制灯指令  0x3C，控制灯指令主要分为两类：一般指示灯和七彩编程呼吸灯。
    
    // 指令说明
    // 功能码：LED 灯模式控制位，1-普通呼吸灯，2-闪烁灯，3-常开灯，4-常闭灯，5-渐开灯，6-渐闭灯，其他功能码不适用于此指令包格式；
    // 起始颜色：设置为普通呼吸灯时，由灭到亮的颜色，只限于普通呼吸灯（功能码 01）功能，其他功能时，与结束颜色保持一致。
    //      其中，bit0 是蓝灯控制位；bit1 是绿灯控制位；bit2 是红灯控制位。置 1 灯亮，置 0 灯灭。
    //      例如 0x01_蓝灯亮，0x02_绿灯亮，0x04_红灯亮，0x06_红绿灯亮，0x05_红蓝灯亮，0x03_绿蓝灯亮，0x07_红绿蓝灯亮，0x00_全灭；
    // 结束颜色：设置为普通呼吸灯时，由亮到灭的颜色，只限于普通呼吸灯（功能码 0x01），其他功能时，与起始颜色保持一致。设置方式与起始颜色一样；
    // 循环次数：表示呼吸或者闪烁灯的次数。当设为 0 时，表示无限循环，当设为其他值时，
    // 表示呼吸有限次数。循环次数适用于呼吸、闪烁功能，其他功能中无效，例如在常开、常闭、渐开和渐闭中是无效的；
    // 指令说明                                                              标识  长度         指令  功能   起始  结束  循环   校     验
    uint8_t PS_COLORLEDBuffer[16]   = { 0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x07, 0x3C, 0x01, 0x02, 0x01, 0x01, 0x00, 0x4A }; // 呼吸灯 绿到蓝色
    uint8_t PS_BlueLEDBuffer[16]    = { 0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x07, 0x3C, 0x03, 0x01, 0x01, 0x00, 0x00, 0x49 }; // 常开 蓝色
    uint8_t PS_RedLEDBuffer[16]     = { 0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x07, 0x3C, 0x02, 0x04, 0x04, 0x02, 0x00, 0x50 }; // 闪烁2次 红色
    uint8_t PS_GreenLEDBuffer[16]   = { 0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x07, 0x3C, 0x02, 0x02, 0x02, 0x02, 0x00, 0x4C }; // 闪烁2次 绿色
    // 更多指令集请参见：http://file.yfrobot.com.cn/datasheet/FPM383C%E6%A8%A1%E7%BB%84%E9%80%9A%E4%BF%A1%E5%8D%8F%E8%AE%AE_V1.2.pdf

    void sendData(int len, uint8_t PS_Databuffer[]);
    void receiveData(uint16_t Timeout);

  public:
    // -----------------------------------------------------------------------------
    // Constructor - YFROBOTFPM383
    // -----------------------------------------------------------------------------
    YFROBOTFPM383(SoftwareSerial *softSerial = NULL);
    SoftwareSerial *_ss;

    void sleep();
    void controlLED(uint8_t PS_ControlLEDBuffer[]);
    uint8_t cancel();
    uint8_t getImage();
    uint8_t getChar();
    uint8_t searchMB();
    uint8_t empty();
    uint8_t autoEnroll(uint16_t PageID);
    uint8_t deleteID(uint16_t PageID);
    uint8_t enroll(uint16_t PageID);
    uint8_t identify();
    uint8_t getSearchID(uint8_t ACK);
    // void ENROLL_ACK_CHECK(uint8_t ACK);

};

#endif // YFROBOTFPM383_H
