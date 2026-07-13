#include <stdlib/fdlibm.h>

double cos(double x) {
    double res;
    __asm__("fcos" : "=t"(res) : "0"(x));
    return res;
}
