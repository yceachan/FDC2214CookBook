# 项目视图

开发环境：Clion armgcc c++17

工具链：   CubeMX +CLION +CMAKE + armgcc +openocd

->FDC_trail

—>cmake-build-debug-embedded--_mingw bulid文件夹

—>Core 软件层

——>bsp 板级支持包，模块算法，外设框架

———>fdc.h/cpp

———>IIC.h/cpp

———>LOG.hpp/cpp

——>INC&SRC CubeMX生成外设初始化，启动文件，main.cpp

—>DRIVER 驱动层

——>HAL库，CMSIS库

—>shadowIOC 影子cubemx工程配置，用来ctrl cv

—>cmsis_dap.cfg  烧录，调试配置文件

