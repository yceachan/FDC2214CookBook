//
// Created by 衣陈 on 2023/3/25.
//

#include "IIC.h"
#include"LOG.hpp"

void delay_us(uint16_t us) {  //极端右值引用优化 ：只传字面量
    __HAL_TIM_SET_COUNTER(DELAYER , 0);
    //减少了不必要的临时变量block和赋值操作。开定时器的操作改为用宏直接写寄存器，减少HAL库的检查消耗。
    __HAL_TIM_ENABLE(DELAYER);
//    HAL_TIM_Base_Start(DELAYER);
//    us*=42;
    while( __HAL_TIM_GET_COUNTER(DELAYER) < us);
//    HAL_TIM_Base_Stop(DELAYER);
    __HAL_TIM_DISABLE(DELAYER);
}
inline void setupDelay() { delay_us(1);}  //用于保证电平变化的顺序    ，建立时间
inline void holdonDelay() { delay_us(2);} //用于SCL HIGH下的 SDA采集，保持时间


void IIC7bitDev::regWrite(uint16_t regADR, uint16_t data) {
    this->start();
    this->sendByte(devADR_WT());

    if(!this->waitAck()) {print("W devADR_WT,Dev noAck\n"); return;}   //


    this->sendByte(regADR);
    if(!this->waitAck()) { print("W regADR,Dev noAck\n");   return;}

    uint8_t  msb=data>>8,lsb=(data<<8)>>8;
    this->sendByte(msb);
    if(!this->waitAck()) { print("W msb,Dev noAck\n");     return;}

    this->sendByte(lsb);
    if(!this->waitAck()) { print("W lsb,Dev noAck\n");     return;}

    this->stop();
}

void IIC7bitDev::regRead(uint16_t regADR, uint8_t len, uint8_t *Rx_buffer) {
    this->start();
    this->sendByte(devADR_WT());
    if(!this->waitAck()) {print("R devADR_WT,Dev noAck\n");  return;}
    this->sendByte(regADR);
    if(!this->waitAck()) { print("R regADR,Dev noAck\n");  return;}

    this->start();
    this->sendByte(devADR_RD());
    if(!this->waitAck()) { print("R devADR_RD,Dev noAck\n");  return;}


    while(len){
        *Rx_buffer++ = this->readByte();
        if(--len) this->ack();
        else      this->nack();
    }
    this->stop();
}


IIC7bitDev::IIC7bitDev(uint16_t devADR, GPIO_TypeDef *sclPORT, uint16_t sclPIN, GPIO_TypeDef *sdaPORT, uint16_t sdaPIN)
        : BaseIICdev(devADR, sclPORT, sclPIN, sdaPORT, sdaPIN) {}


BaseIICdev::PIN_STATE BaseIICdev::read_SDA() {
    return PIN_STATE(HAL_GPIO_ReadPin(SDA_PORT,SDA_PIN));
}

uint16_t BaseIICdev::devADR_RD() {
    return ((this->devADR<<1)|1);
}

uint16_t BaseIICdev::devADR_WT() {
    return ((this->devADR<<1)|0);;
}



void BaseIICdev::start() {
    this->set_SDA(HIGH);;//tf=300ns no setup for data
    this->set_SCL(HIGH);
    setupDelay();//Setup time for start condition:0.6
    this->set_SDA(LOW);
    holdonDelay();//Hold time for start condition  0.6 fast
    this->set_SCL(LOW);
    setupDelay();//SCL low period:1.3us,SCL低的剩余延时在下一个SDA的setup中。
}

void BaseIICdev::stop() {
    this->set_SDA(LOW);//tf=300ns no setup for data
    this->set_SCL(HIGH);
    setupDelay();//Setup time for a stop condition:0.6s
    this->set_SDA(HIGH);
    holdonDelay();//SDA high pulse duration between STOP and START :1.3us ：1.3us的holdon SDA采集
//    this->set_SCL(LOW);   通讯结束，空闲释放SCL即可,无需拉低SCL。
//    holdonDelay();//SCL low period:1.3us
}

void BaseIICdev::ack(){
    this->set_SDA(LOW); //���ݴ�����
    setupDelay();
    this->set_SCL(HIGH);
    holdonDelay();
    this->set_SCL(LOW);//SCL被拉低后不需要holdon把
    setupDelay();
    this->set_SDA(HIGH);//主机完成SDA低应答后，释放SDA
}
void BaseIICdev::nack() {
    this->set_SDA(HIGH);
    setupDelay();
    this->set_SCL(HIGH);
    holdonDelay();
    this->set_SCL(LOW); //主机nack非应答后，仅接stop时序，故拉低SCL
    setupDelay();
}

void BaseIICdev::sendByte(uint8_t data){
    /*0x80 = 0b1000,0000 即取得u8首位  */
    for(uint8_t i=0;i<8;i++)
    {

        set_SDA( ((data & 0x80)) ? HIGH : LOW );    setupDelay();   //无需移位，0b1000,0000 = true
        set_SCL(HIGH);  holdonDelay();
        data<<=1;
        set_SCL(LOW);setupDelay();  //SCL LOW要求1.3us,配合SDA的setupDelay 正好
    }
    set_SDA(HIGH);//�ͷ�SDA
}

uint8_t BaseIICdev::readByte() {
    uint8_t Rx=0;
    for(uint8_t i=0;i<8;i++)
    {
        Rx<<=1;
        set_SCL(HIGH);  holdonDelay();
        Rx|=this->read_SDA();
        set_SCL(LOW);  holdonDelay();
    }
    return Rx;
}

bool BaseIICdev::waitAck() {  //
    PIN_STATE  re = HIGH;

    set_SDA(HIGH);	/* 释放SDA，此时SDA电平为跟从机SDA线与结果，若从机下拉SDA应答，则为低，若尚未应答，则为高 */
    setupDelay();
    set_SCL(HIGH);	/* 产生*/

    __HAL_TIM_SET_COUNTER(DELAYER,0);
    __HAL_TIM_ENABLE(DELAYER);
    while( ( (re=read_SDA()) == HIGH ) && __HAL_TIM_GET_COUNTER(DELAYER) < 30000 );

    __HAL_TIM_DISABLE(DELAYER);

    set_SCL(LOW); //为stop时序准备的SCL低
    return (re == LOW);
}
BaseIICdev::BaseIICdev(uint16_t devADR, GPIO_TypeDef *sclPORT, uint16_t sclPIN, GPIO_TypeDef *sdaPORT, uint16_t sdaPIN) {
    this->devADR=devADR;
    this->SCL_PIN=sclPIN;
    this->SCL_PORT=sclPORT;
    this->SDA_PIN=sdaPIN;
    this->SDA_PORT=sdaPORT;
}

void BaseIICdev::set_SCL(BaseIICdev::PIN_STATE op) {
    HAL_GPIO_WritePin(SCL_PORT,SCL_PIN,(GPIO_PinState)op);
}

void BaseIICdev::set_SDA(BaseIICdev::PIN_STATE op) {
    HAL_GPIO_WritePin(SDA_PORT,SDA_PIN,(GPIO_PinState)op);
}
