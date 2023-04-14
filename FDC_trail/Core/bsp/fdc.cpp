//
// Created by 衣陈 on 2023/3/25.
//

#include "fdc.h"
#include <cmath>
#include <vector>
#include<algorithm>
#include <numeric>
/*FDC2214    iic从地址
 *ADDR = L , I2C Address = 0x2A
 *ADDR = H , I2C Address = 0x2B*/
#define FDC2214_ADDR 0x2A

/*FDC2214各个寄存器地址*/
#define DATA_CH0 0x00     				//数据寄存器
#define DATA_LSB_CH0 0x01
#define DATA_CH1 0x02
#define DATA_LSB_CH1 0x03
#define DATA_CH2 0x04
#define DATA_LSB_CH2 0x05
#define DATA_CH3 0x06
#define DATA_LSB_CH3 0x07
#define RCOUNT_CH0 0x08    //引用计数转换间隔时间设置，
#define RCOUNT_CH1 0x09
#define RCOUNT_CH2 0x0A
#define RCOUNT_CH3 0x0B
//#define OFFSET_CH0 0x0C  //FDC2114
//#define OFFSET_CH1 0x0D
//#define OFFSET_CH2 0x0E
//#define OFFSET_CH3 0x0F
#define SETTLECOUNT_CH0 0x10    //震荡稳定时间设置
#define SETTLECOUNT_CH1 0x11
#define SETTLECOUNT_CH2 0x12
#define SETTLECOUNT_CH3 0x13
#define CLOCK_DIVIDERS_C_CH0 0x14       //时钟分频
#define CLOCK_DIVIDERS_C_CH1 0x15
#define CLOCK_DIVIDERS_C_CH2 0x16
#define CLOCK_DIVIDERS_C_CH3 0x17
#define STATUS 0x18                     //状态寄存器
#define ERROR_CONFIG 0x19 				//错误报告设置
#define CONFIG 0x1A
#define MUX_CONFIG 0x1B
#define RESET_DEV 0x1C
#define DRIVE_CURRENT_CH0 0x1E          //电流驱动
#define DRIVE_CURRENT_CH1 0x1F
#define DRIVE_CURRENT_CH2 0x20
#define DRIVE_CURRENT_CH3 0x21
#define MANUFACTURER_ID 0x7E      //读取值：0x5449
#define DEVICE_ID 0x7F            //读取值：0x3055
#include "LOG.hpp"
void Fdc::singleinit() {
    this->regWrite(RESET_DEV,0x8000);//复位

    uint8_t did[2]={0};
    this->regRead(DEVICE_ID, 2, did);
    uint16_t  DID = (did[0] << 8 | did[1] );  //整形提升
    LOG(DID);//读出0x3055符合预期

    uint8_t mid[2]={0};
    this->regRead(MANUFACTURER_ID, 2, mid);
    uint16_t  MID = (mid[0] << 8 | mid[1] );  //整形提升
    LOG(MID);//读出0x5449符合预期


    //100us/sample ::measureTime=4k*Tref(40m^-1) , reg_DATA ::0xFA * 16u * Tref =2kTREF=100us/sample
    this->regWrite(RCOUNT_CH2,0x0200);  //nobt=0xF00 = 12位有效数字

    //等待沉降时间推算CHx_SETTLECOUNT > Vpk × fREFx × C × π /32 /IDRIVE=4.499
    this->regWrite(SETTLECOUNT_CH2,0x002C);//(T=0x0a * 16 /fREF)=等待沉降时间4us

    this->regWrite(CLOCK_DIVIDERS_C_CH2,0x1002);//Fin1分频0.01-8.75mhz，fREF1分频 10mhz
    this->regWrite(DRIVE_CURRENT_CH2,0xF000);//电流驱动:0.146mA（传感器时钟建立+转换时间的驱动电流,驱动电流da，转换快）

//    this->regWrite(MUX_CONFIG,0x820D);//滤波带宽10mhz
    this->regWrite(MUX_CONFIG,0x020D);//滤波带宽10mhz
    this->regWrite(CONFIG,0x9C01);//
}

std::tuple<double, double, double, uint32_t> Fdc::plot_test(uint8_t CH) {

    uint8_t rx[2]={0};
    bool UNREADCONV=false;
    uint8_t msb[2]={0},lsb[2]={0};
    uint32_t cnt=0;
    __HAL_TIM_SET_COUNTER(&htim6,0);
    HAL_TIM_Base_Start(&htim6);
    do {
        this->regRead(STATUS,2,rx);
        UNREADCONV=rx[1] & ( CH?(0b00000100):(0b00000010) );
        //降低轮询频率，防止IIC错误
        delay_us(100);
    }while(!UNREADCONV);

    this->regRead(CH ? DATA_CH1 : DATA_CH2,2,msb);
    this->regRead(CH ? DATA_LSB_CH1 :DATA_LSB_CH2,2,lsb);

    cnt= __HAL_TIM_GET_COUNTER(&htim6);
    HAL_TIM_Base_Stop(&htim6);

    uint32_t data=0;
    data|= (msb[0]<<(24)) | (msb[1]<<16) | (lsb[0]<<8) | (lsb[1]);
    data<<=4;
    data>>=4;
    double f=1*20*data/double(1<<28);//
    //f=1/(2 * pi * sqrt{ L * C } )
    //C=1/( (2 * pi * f )^2 * L)
    double c_max = (1/(pow(2*acos(-1)*f,2) * 18*0.9 ))  * 1e6;//板载电感:18uh 板载电容:__pf

    double c_min = (1/(pow(2*acos(-1)*f,2) * 18*1.1 ))  * 1e6;//板载电感:18uh 板载电容:__pf

    return {c_min,c_max,f,cnt};
}

void Fdc::refinit() {

}

void Fdc::refplot_test() {

}


