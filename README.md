# FDC2214使用亦c++嵌入式开发体验

> 本项目是笔者对FDC2214电容测量模块的学习归档，亦是笔者使用现代stm32开发框架（CLion+armgcc+openOcd）的第一个项目。在此归档，仅供参考。

本项目内容如下：

* 硬件：

  * fdc2214模块官方资料，应用笔记，误差分析。

* 软件：

  * c++17开发stm32的一些trick，包括不限于：

    面向对象的软件iic，泛型print方法，LOG宏魔法

# 1.软件框架：

实验平台：stm32f407IGT6,HAL库驱动

## 1.1 IDE环境

本项目使用 CubeMX+Clion + armgcc +openocd工具链开发，相比传统的keil开发或vscode编辑，keil调试方案，

兼具现代IDE的高级特性（如，代码补全，高亮提示，代码重构）

和基于openOCD的单步调试功能（可直接监视外设）

并且，使用C++17语言标准，使用了许多C++高级语言特性，如元组拆包，泛型编程，使得嵌入式开发体验极佳。

（然而听说fpga的ide又很简陋呢,keil6年底也要除了....）

参考稚晖博客[配置CLion用于STM32开发【优雅の嵌入式开发】 - 知乎 (zhihu.com)](https://zhuanlan.zhihu.com/p/145801160)

## 1.2 软件trick

### 1.面向对象的软件iic

> * 
> * 面向对象的软件IIC框架，使用继承特性保证程序 对不同地址长度IIC设备的 可扩展性
> * 基类BaseIICdev实现了IIC通信的基本时序和对SCL，SDA的GPIO操作封装。此层对HAL库的封装保障了框架在不同平台的移植性，修改set_PIN底层即可
> * 派生类应在基类实现了基本时序的基础上，实现读写寄存器的操作。在本框架中，暂时只实现了最常见的7bit IIC设备。
> * 本框架使用方法：IIC通信需要微妙延时函数满足时序。框架应传入配置为1mhz的计时器句柄（在下方修改）
> * *关于中断管理，根据IIC时序特性，若延时函数被长时中断拉长，通讯一般不会出错，但速率会下降。
> * 若iic设备对实时通信有需求，应修改框架配置中断管理，在时序函数中失能中断。
> * 在waitAck函数中，设置了30ms等待时间，若在此函数中发生中断，可能会导致主机错失应答信号或等待超时等非预期错误，建议失能中断。
> * 本框架设置了两个延时函数：setupDelay=1us,holdonDelay=2us,可认为IIC通讯速率333khz。
> * 这两个延时的配置是笔者对IIC协议中，建立时间，保持时间的学习体会。笔者发现现有网上例程往往都用同一个delay一笔带过，不 优 雅 ！
> */

~~~cpp
#ifndef FDC_TRAIL_IIC_H
#define FDC_TRAIL_IIC_H
#include "tim.h"
#include"main.h"

#define DELAYER &htim7
void delay_us(uint16_t us);


class BaseIICdev{
public:
    BaseIICdev(uint16_t devADR, GPIO_TypeDef* sclPORT, uint16_t sclPIN, GPIO_TypeDef* sdaPORT, uint16_t sdaPIN);
    virtual void regWrite(uint16_t regADR, uint16_t data)=0;
    virtual void regRead(uint16_t regADR, uint8_t len, uint8_t* Rx_buffer)=0;
    uint16_t devADR;
    GPIO_TypeDef *SCL_PORT,*SDA_PORT;
    uint16_t SCL_PIN,SDA_PIN;
protected:
    void start();
    void stop();

    enum  ACK_CHOICE{ACK=0,NACK=1};
    enum  PIN_STATE {LOW=0,HIGH=1};

    void sendByte(uint8_t data);
    uint8_t readByte();
    void ack();
    void nack();

    bool waitAck();


    inline uint16_t devADR_RD();
    inline uint16_t devADR_WT();

    inline void set_SCL(PIN_STATE op);
    inline void set_SDA(PIN_STATE op);
    inline PIN_STATE read_SDA();
};


class IIC7bitDev : public BaseIICdev{
public:
    IIC7bitDev(uint16_t devADR, GPIO_TypeDef* sclPORT, uint16_t sclPIN, GPIO_TypeDef* sdaPORT, uint16_t sdaPIN);
    virtual void regWrite(uint16_t regADR, uint16_t data);
    virtual void regRead(uint16_t regADR, uint8_t len, uint8_t* Rx_buffer);
};


#endif //FDC_TRAIL_IIC_H

~~~



### 2.trick:泛型print

armgcc配置下，printf重定向较繁琐，c风格的格式串不优雅，cpp20的format库用不了也还没学，于是笔者捣鼓出了这套串口print框架，

类型安全，打印数值自动空格，不必写格式串，也可惜不能格式串控制。

亦是笔者的泛型编程最佳实践，学到了左右值，万能引用，初始化列表，lamda拆包等**Acknowledge**和**Trick**和**Dark Magic**

不作展开，有空出本cppCookBook细说捏

~~~cpp
#define LOGGER &huart1 //传入已初始化完成的串口句柄
//泛型策略：通过is_assignable_v<string>的谓词在模板中处理好数值和字符串
//然后为char(按asiic而非signed8打印)和bool生成特化方法。
template<typename T>
void print(T&& arg){
    if constexpr (std::is_assignable_v<std::string_view ,T>){   //能顺利捕捉到char (&)[]类型的方法。
        std::string_view str=arg;
        HAL_UART_Transmit(LOGGER,str.data(),str.length(),300);
        bool oh=std::is_arithmetic_v<char>;
    }
    else {
        std::string str=std::to_string(arg)+' ';
        HAL_UART_Transmit(LOGGER,str.c_str(),str.length(),300);
    }
}
//字符与bool的特化方法，区分左右值引用
template<> void print<const char&>(const char& ch);
template<> void print<const char&&>(const char&& ch);
template<> void print<const bool&>(const bool& ch);
template<> void print<const bool&&>(const bool&& ch);

//初始化列表+lamda表达式+...解包の黑魔法
template<typename... Ts>
void print(Ts&&... args) {
    (void) std::initializer_list<int> { ( [&]{print(args);}(),0 )... };
}

~~~

**Q:对于print的多个重载，函数调用的入口的仲裁逻辑是？**

期待cppCookBook捏

### 3.LOG宏魔法

**懒狗笔者有如下需求：实现优雅的log方案，可以传入不定长个需打印的变量，打印当前函数名，行号，每个变量的名与值。**

笔者从一个cpp交流群中偷来了这个魔法，

在宏定义#define中，有如下宏指令

~~~cpp
//...传包和__VA_ARGS__宏拆包，注意，不同于模版，仅是文本替换。
#define logplot(...) do{print("sample:",__VA_ARGS__,'\n');}while(0)

//#解宏参数，将文本替换为参数名的字符串而非参数值
#define LOG(A) {print(#A,':',A,endl);}  

//一个可以嵌套展开宏指令的简单宏魔法
#define EXPAND(args) args
~~~

组合起来就有了以下logln宏魔法，支持最多10通道的泛型log

(不能支持任意多个，但无需额外实现函数，性能优秀。)

~~~cpp
/*
* @name   LOG
* @brief  单具名变量LOG方法，打印此变量的名与值，移植到stm32，底层print是HAL库串口发送
* @param  print_able Lvalue
* @notice 将打印传入此方法的文本，可以是表达式或字面量，前者你可用以表达式求值
* @Author 群佬
*/
#define LOG(A) {print(#A,':',A,endl);}
#define LOG1(_1) LOG(_1)
#define LOG2(_1,...) LOG(_1); LOG1(__VA_ARGS__)
#define LOG3(_1,...) LOG(_1); LOG2(__VA_ARGS__)
#define LOG4(_1,...) LOG(_1); LOG3(__VA_ARGS__)
#define LOG5(_1,...) LOG(_1); LOG4(__VA_ARGS__)
#define LOG6(_1,...) LOG(_1); LOG5(__VA_ARGS__)
#define LOG7(_1,...) LOG(_1); LOG6(__VA_ARGS__)
#define LOG8(_1,...) LOG(_1); LOG7(__VA_ARGS__)
#define LOG9(_1,...) LOG(_1); LOG8(__VA_ARGS__)
#define LOG10(_1,...) LOG(_1); LOG9(__VA_ARGS__)

#define _GET_NTH_ARG(_1,_2,_3,_4,_5,_6,_7,_8,_9,_10, N,...) N //返回传参的第11个参数的文本
#define NTH_LOG(...) _GET_NTH_ARG(__VA_ARGS__,LOG10,LOG9,LOG8,LOG7,LOG6,LOG5,LOG4,LOG3,LOG2,LOG1)//传参的第11个参数是将要分发的LOGx方法

#define EXPAND(args) args
/*
 * @name  logln
 * @brief 一个接受任意个PRINTABLE参数，然后逐个逐行打印其名与值的宏魔法
 * @param  print_able * any
 * @notice：打印名将是传入文本的原样,可以为表达式或字面量;底层LOGGER为print方法，请实现,
 * */
#define logln(...) do{print("in ",__FUNCTION__,",l",__LINE__,endl);EXPAND(NTH_LOG(__VA_ARGS__))(__VA_ARGS__);}while(0)
#define logplot(...) do{print("sample:",__VA_ARGS__,'\n');}while(0)
~~~

# 2. FDC调试报告

> 经多方排除地，一度以为是模块烧了地，笔者手上的模块的板载电感容差为10%，电感L会在LC震荡换算公式中乘上较大的系数，使得测量误差很夸张。但这个FDC测的还是很稳很快的，就是永远找不到真值咯。

>  f可作时钟。故频率是可用定时器测量

![image-20230405182446885](https://s2.loli.net/2023/04/14/p4Soir1VgB27yFP.png)

**笔者的评估板除了soc上集成的33pf贴边电容外，还并联了一块电容值未知的金属极板，用以做接触检测等。**

**此外，师兄焊了个容差为10%的电感上去，就只能当大玩具了**

## 2.1 SYS框图

![image-20230309235332784](https://s2.loli.net/2023/03/09/3kVeRn5m8LzGKZy.png)

$F_{CLK}=40Mhz$，使用内置时钟源

![image-20230309234745616](https://s2.loli.net/2023/03/09/SrHswRi1oBU2JGp.png)

模块上集成一个贴片电感，一个贴片电容，一个金属极板。可采用**差分方式Differential**测量外部电容，暨，在INAB并联上待测量电容得$L(C_{cap} + C_{tar})$ 振荡器，其震荡出$f_{SENSOR}$，如上所述f可做时钟用以测量。



$f_{SENSOR}$经FIN_SEL分频（寄存器配置），向CORE输入$f_{IN}=f_{SENSOR0 }/ CHx\_FIN\_SEL$



### 2.1.1 **传感端**

![image-20230310000848359](https://s2.loli.net/2023/03/10/SY1nCB9Da6IcdxA.png)

通常，FDC上的LCtank为 18uH，33pF，得6.5Mhz的f~CAP~

(非TI规范) 

jlc原理图如下

![image-20230310004427175](https://s2.loli.net/2023/03/10/SYBJ45FMNVZ8rvw.png)

### 2.1.2 输出&滤波端

不带分频因子

![image-20230310005406155](https://s2.loli.net/2023/03/10/H6qYgdNsC7mTcAl.png) 

带分频因子

![image-20230310005901424](https://s2.loli.net/2023/03/10/aTidyMVm3EkFjCL.png)



**Gain and OFFSET 数字增益和偏移调整**

* C = sensor capacitance including parasitic PCB capacitance C为全寄生电容包括PCB寄生电容，不包括模块集成LCtank，

* 在本公式中，Csensor指

  ==当然2214已经做好了这部分滤波，只需要把DATAx简单换算分频因子即可，所以看个乐呵==

![image-20230310002952284](https://s2.loli.net/2023/03/10/5vxSotgjMBKfX7V.png)

**Clock cofiguration requirement**

![image-20230309235019215](https://s2.loli.net/2023/03/09/QKF1rapB9xWSyVu.png)

![image-20230309235140436](https://s2.loli.net/2023/03/09/uVoL9ZF5wRleGbT.png)

**最终换算公式**

$f_{SENSOR}=FIN_{SEL}*f_{REF}*DATAx  /2^{28}=\frac{1}{2\pi \sqrt{L(C_{cap}+C{tar})}}$

$L=18uH,C_{cap}=33pf$

Csensor=.....

### 2.1.3 后端——fIN计数器

![image-20230414192622428](https://s2.loli.net/2023/04/14/KdjkIsiWO5nENb4.png)

fdc文档里好像没有fin测量原理这一段，但根据传感器返回结果是fin与fref的比值，以及RCOUT寄存器配置这些，很好想f是怎么测出的。

## 2.2 寄存器map暨软件任务

~~~cpp
void Fdc::singleinit() {
    //复位命令寄存器，当写入1在最高位时复位
    this->regWrite(RESET_DEV,0x8000);

    uint8_t did[2]={0};
    //设备ID寄存器
    this->regRead(DEVICE_ID, 2, did);   
    uint16_t  DID = (did[0] << 8 | did[1] );  //整形提升
    LOG(DID);//读出0x3055符合预期

    uint8_t mid[2]={0};
     //制造ID寄存器，两个自检
    this->regRead(MANUFACTURER_ID, 2, mid);   
    uint16_t  MID = (mid[0] << 8 | mid[1] );  //整形提升
    LOG(MID);//读出0x5449符合预期

    //测量周期配置寄存器：测量RCOUNT*16个fref周期，
    this->regWrite(RCOUNT_CH2,0x0200);//100us/sample ::measureTime=4k*Tref(40m^-1) , reg_DATA ::0xFA * 16u * Tref =2kTREF=100us/sample
    
    //等待沉降时间寄存器：LC启震，需要时间稳定，芯片通过写死等待时间来滤去这段时间的f
    this->regWrite(SETTLECOUNT_CH2,0x002C); (T=0x0a * 16 /fREF)//等待沉降时间推算CHx_SETTLECOUNT > Vpk × fREFx × C × π /32 /IDRIVE=4.499 等待沉降时间4us
    
    //时钟配置寄存器
    this->regWrite(CLOCK_DIVIDERS_C_CH2,0x1002);//Fin1分频，fREF1分频 10mhz
    //电流驱动寄存器
    this->regWrite(DRIVE_CURRENT_CH2,0xF000);//电流驱动:1.xxmA（传感器时钟建立+转换时间的驱动电流,驱动电流da，转换快）

    //通道选取与滤波配置寄存器
//    this->regWrite(MUX_CONFIG,0x820D);//多通道模式，滤波带宽10mhz
    this->regWrite(MUX_CONFIG,0x020D);//单通道模式，滤波带宽10mhz
    
    //一些杂七杂八还有解除休眠的寄存器。
    this->regWrite(CONFIG,0x9C01);
}

//虽然fdc的测量原理支持软件直接轮询数据寄存器，但还是建议读取此寄存器判断数据是否准备好，
//还有一个通道数据READY寄存器
 do {
        this->regRead(STATUS,2,rx);
        UNREADCONV=rx[1] & ( CH?(0b00000100):(0b00000010) );
        //降低轮询频率，防止IIC错误
        delay_us(100);
    }while(!UNREADCONV);
~~~

具体配置还是请参阅IC手册。文档**10.2 Typical Application**中有推荐寄存器配置。

文档**9.4 Device Functional Modes**中提及模块上电后处于休眠模式，需写CONFIG寄存器解除

（问就是踩过坑呜呜~~）

![image-20230414194847819](https://s2.loli.net/2023/04/14/Nbfs9EUMLq7YGwC.png)

![image-20230414195413754](https://s2.loli.net/2023/04/14/XsDTNmw7iVJnC8A.png)

## 2. IIC时序要求

400khz fast mode 如下

![image-20230411125440601](https://s2.loli.net/2023/04/11/mIlpxH8NT7bOXnZ.png)

# 3.FDC测量报告：

## 3.1 误差分析

此FDC模块测量示值距离标称值较远，始终偏大，误差分析如下：



1. ==致命误差：电感容差10%，使得精准测量不再现实==

   

2. 电容是最不抗干扰的模块，受外界干扰程度很强，如，环境湿度，测量场地等，甚至接线座子，面包板，杜邦线的连接都会为传感端加上约1pf的等效电容。（注意1pf=1-e12F捏。

> 》2.1不同端子接线下，零值有1pf的差值

<img src="https://s2.loli.net/2023/04/14/xpX6HgrV2RqhL3A.png" alt="image-20230414200434619" style="zoom: 15%;" />

> 》2.2在纸张测量实验中，测量外部极板电容，极板会有较大电磁干扰

![image-20230414202611487](https://s2.loli.net/2023/04/14/vhtK48HnIWlScpY.png)





3. 电容具有频率特性，其上有等效串联电阻，等效串联电感，在不同频率下电容真值不尽相同

 <img src="https://s2.loli.net/2023/04/14/TXOV7HrnN5SywcJ.png" alt="image-20230414222403469" style="zoom: 50%;" /><img src="https://s2.loli.net/2023/04/14/swRh4CEbK9aPgGU.png" alt="image-20230414222628162" style="zoom: 67%;" />





4. 极板充电效应。根据19年电赛纸张测量论文，极板长时间未短路时会产生充电效应，使板载电容增大。

![image-20230414201632432](https://s2.loli.net/2023/04/14/yEZAOqxCH7s983h.png)



==而评估板soc上以**单端模式**(single -ended)串联了一个等效电容未知的极板==

![image-20230414202243980](https://s2.loli.net/2023/04/14/CkdSc2w8P6ngUGh.png)





## 3.2 测量速率分析

配置RCOUNT和SETTLE，可编程测量速率，理论最大速率：

$(0XFF +0X0A)*16/fREF=4240/40Mhz  =106 us/sample$

实际测量，包括IIC通信时间，126us/sample  测量程序捏，飞了。

<img src="https://s2.loli.net/2023/04/14/hUApjv4zRHOyeYb.png" alt="image-20230414203640543" style="zoom: 50%;" />

## 3.3 可视化测量记录

### 3.3.1 纸张计数实验

> 纸张数-》电容记录表，早期实验，采样方式不科学（串口读数，一眼顶针法）

<img src="https://s2.loli.net/2023/04/14/LGXzFUP3QkxgNhu.png" style="zoom: 33%;" />

> 线性插值曲线与模拟测试。C与d的反比例关系后期精度不了，数据有一个坏点。

<img src="https://s2.loli.net/2023/04/14/XTwJdk52Db4Y3Bf.png" alt="image-20230414203910656" style="zoom: 33%;" />

> MATLAB源码，选取部分样点生成分段线性插值模型，模拟测试输入为记录表中数据添加正负0.08pf的噪声。（下面的程序还没加上噪声，加了结果不变，还是有个坏点。）

<img src="https://s2.loli.net/2023/04/14/hiXRIYjvHpZ23FG.png" alt="image-20230414204039580" style="zoom: 67%;" />



![image-20230414204422104](https://s2.loli.net/2023/04/14/FDnobUmGhSQYKw5.png)

### 3.3.2一些电容测量数据

> 对一些小电容的测量，误差与测量电容规模成比例关系

<img src="https://s2.loli.net/2023/04/14/dHLbVhI6tj38J2U.png" alt="image-20230414220528553" style="zoom:67%;" />

> 发现模块集成电感容差为10%，为输出数据除补偿因子{0.9 ,1.1 }，示值的上界下界如图：

<img src="https://s2.loli.net/2023/04/14/SlnRj5mCdb9HyT1.png" alt="image-20230414221926728" style="zoom: 33%;" />

## 3.4 TI参考译文贴

### 3.4.1 @驱动电流可能影响电容示值

![image-20230415203531365](https://s2.loli.net/2023/04/15/6lHwmtzFyxZqhvK.png)

见3.4.3.电流源驱动影响测量电压，低电压精度不足，高电压会错误打开意

### 3.4.2@fdc不适合测量绝对电容

![image-20230415205005706](C:\Users\yceachan\AppData\Roaming\Typora\typora-user-images\image-20230415205005706.png)

### 3.4.3@电容的阻抗-频率特性可能影响测量结果

![image-20230415211204142](https://s2.loli.net/2023/04/15/MLt4oy1cp2G5Hah.png)