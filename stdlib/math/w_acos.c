#include <stdlib/fdlibm.h>

double acos(double x) {
    return __ieee754_atan2(__ieee754_sqrt(1.0 - x * x), x);
}
