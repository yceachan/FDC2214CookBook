软件IIC速率札记

> 本札记测量了笔者的软件IIC框架在各种配置下，在执行两个regRead函数共72个IIC时钟下的表现。
>
> 在本项目IIC.h/cpp可找到对应框架。目前性能

# 测速

> 测量方式：读取fdc的两颗数据寄存器，一共需要写4个byte，读4个byte，总共72个时钟 。
>
> 测量环境：o0优化
>
> 测量参量：IIC框架是否使用虚函数，IIC延时中开定时器方式
>
> 测量对象：regRead耗时，主要影响因素是软延时延时时间4us
>
> ```cpp
> __HAL_TIM_SET_COUNTER(&htim6,0);
> __HAL_TIM_ENABLE(&htim6);
> this->regRead( DATA_CH2,2,msb);
> this->regRead(DATA_LSB_CH2,2,lsb);
> T= __HAL_TIM_GET_COUNTER(&htim6);
> __HAL_TIM_DISABLE(&htim6);
> ```

## **未优化多态 ，未优化软延时 872us**  

![image-20230420144930705](https://s2.loli.net/2023/04/20/LSeRHPbzG9AVql2.png)

![image-20230420145041728](https://s2.loli.net/2023/04/20/nTVd3cYeQlZjoG9.png)

## 未优化多态，优化软延时 687us

![image-20230420145220272](https://s2.loli.net/2023/04/20/Et4n1uDzwI7QvfZ.png)

![image-20230420145257843](https://s2.loli.net/2023/04/20/1YO27podJLiMyGt.png)

## 去除多态，优化软延时 687us

![image-20230420145423110](https://s2.loli.net/2023/04/20/KGANRMm6OCyBdcH.png)

面向对象的多态设计并未带来明显开销（在基类为纯虚函数下）

## 软延时保留临时变量赋值 688us

![image-20230420145809195](https://s2.loli.net/2023/04/20/5M9aIHXicJ6LOAl.png)

读写内存的速率符合预期，168mhz的PC主频不会对0.33mhz的IIC时钟产生太大影响

## 软延时重新引入HAL库开定时器（同实验1） 872us

![image-20230420150106062](https://s2.loli.net/2023/04/20/fTFQ3G8POADgpeu.png)

 

# HAL_TIM_START/STOP 实现如下：

~~~cpp
HAL_StatusTypeDef HAL_TIM_Base_Start(TIM_HandleTypeDef *htim)
{
  uint32_t tmpsmcr;

  /* Check the parameters */
  assert_param(IS_TIM_INSTANCE(htim->Instance));

  /* Check the TIM state */
  if (htim->State != HAL_TIM_STATE_READY)
  {
    return HAL_ERROR;
  }

  /* Set the TIM state */
  htim->State = HAL_TIM_STATE_BUSY;

  /* Enable the Peripheral, except in trigger mode where enable is automatically done with trigger */
  if (IS_TIM_SLAVE_INSTANCE(htim->Instance))
  {
    tmpsmcr = htim->Instance->SMCR & TIM_SMCR_SMS;
    if (!IS_TIM_SLAVEMODE_TRIGGER_ENABLED(tmpsmcr))
    {
      __HAL_TIM_ENABLE(htim);
    }
  }
  else
  {
    __HAL_TIM_ENABLE(htim);
  }

  /* Return function status */
  return HAL_OK;
}
HAL_StatusTypeDef HAL_TIM_Base_Stop(TIM_HandleTypeDef *htim)
{
  /* Check the parameters */
  assert_param(IS_TIM_INSTANCE(htim->Instance));

  /* Disable the Peripheral */
  __HAL_TIM_DISABLE(htim);

  /* Set the TIM state */
  htim->State = HAL_TIM_STATE_READY;

  /* Return function status */
  return HAL_OK;
}

~~~

#  `assert_param(IS_TIM_INSTANCE(htim->Instance));`

> ```
> #define assert_param(expr) ((expr) ? (void)0U : assert_failed((uint8_t *)__FILE__, __LINE__))
> ```

![image-20230420150646328](https://s2.loli.net/2023/04/20/8LeMQUo7TcExJtd.png)

![image-20230420150854793](https://s2.loli.net/2023/04/20/wapd7gTWzMIrVB1.png)

## 都只是读写以下寄存器，好像没有需要耗时100us的操作啊

# 猜想解释：读定时器寄存器的速率是？

是APB时钟还是分频时钟呢？

如果是分频时钟1mhz，那么额外耗时可以解释的 **（但这是相当反直觉的**

![image-20230420153605705](https://s2.loli.net/2023/04/20/RUSn94D6GA5ye3j.png)

使用HAL库开启定时器会多读一个寄存器，可以折合1us。

72个时钟多耗时100多us

根据这个模型，对于872us的计算，此处1us

一个时钟2个延时共3us，开关两次定时器4us，根据读一次定时器1us

`while( (block=__HAL_TIM_GET_COUNTER(DELAYER)) < us);` 共执行3次us

共12us,合83.3khz，72个时钟即为864us 

听起来比较合理

去掉hal库，11us合90.9khz,72个时钟即为654us

听起来比较合理。

**但很假**

## 验证方案：测试84mhz定时器在两个软延时的性能

理论而言，配置84mhz后，二者都会在while中阻塞多次，HAL函数的检查开销的影响会变小，二者耗时应该相近

## 宏开定时器 843us

![image-20230420170625920](https://s2.loli.net/2023/04/20/qD5YViftSF9gWZH.png)

## HAL开定时器  1048us

![image-20230420170826079](https://s2.loli.net/2023/04/20/KQrLsIXFcu2oWVJ.png)

# 以上猜想有假，两个函数性能均下降，且差距拉大

to be continued.....

# 软件IIC优化：

明确了setup和holdon逻辑，优化IIC框架时序

**当前IIC配速：250khz**

理论读取用时为：72/250khz = 288us

## HAL开定时器1mhz，优化IIC 809us

![image-20230420174155957](https://s2.loli.net/2023/04/20/LnozIAvFxJhMgwe.png)

## 宏开定时器1mhz 优化IIC 626us=115khz

![image-20230420174403627](https://s2.loli.net/2023/04/20/eZPtOXpazA5Vfhw.png)

## 宏开定时器42mhz 优化IIC 782us

`htim7.Init.Prescaler = 1;`

![image-20230420175154634](https://s2.loli.net/2023/04/20/QXcNvWA27LZeUr6.png)



疑惑是更高分辨率的寄存器为什么软延时性能反而更差