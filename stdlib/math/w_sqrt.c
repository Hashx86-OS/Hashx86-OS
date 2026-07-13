/* wrapper sqrt(x) */
#include <stdlib/fdlibm.h>

double sqrt(double x) {
    return __ieee754_sqrt(x);
}
