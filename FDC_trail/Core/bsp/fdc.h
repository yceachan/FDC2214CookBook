//
// Created by 衣陈 on 2023/3/25.
//

#ifndef FDC_TRAIL_FDC_H
#define FDC_TRAIL_FDC_H

#include <tuple>
#include "IIC.h"
#include "main.h"
class Fdc: public IIC7bitDev  {
public:
    using IIC7bitDev::IIC7bitDev;
    void singleinit();
    void multinit();
    void test();
    std::tuple<double, uint32_t> plot_test();

    void refinit();
    void refplot_test();
};


#endif //FDC_TRAIL_FDC_H
