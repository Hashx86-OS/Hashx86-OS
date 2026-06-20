/* @(#)s_scalbn.c 1.3 95/01/18 */
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunSoft, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice 
 * is preserved.
 * ====================================================
 */

#include <stdlib/fdlibm.h>

#ifdef __STDC__
static const double
#else
static double
#endif
two54   =  1.80143985094819840000e+16,
twom54  =  5.55111512312578270212e-17,
huge   = 1.0e+300,
tiny   = 1.0e-300;

#ifdef __STDC__
	double scalbn (double x, int n)
#else
	double scalbn (x,n)
	double x; int n;
#endif
{
	int  k,hx,lx;
	hx = __HI(x);
	lx = __LO(x);
        k = (hx&0x7ff00000)>>20;
        if (k==0) {
            if ((lx|(hx&0x7fffffff))==0) return x;
	    x *= two54; 
	    hx = __HI(x);
	    k = ((hx&0x7ff00000)>>20) - 54; 
            if (n< -50000) return tiny*x;
	    }
        if (k==0x7ff) return x+x;
        k = k+n; 
        if (k >  0x7fe) return huge*copysign(huge,x);
        if (k > 0)
	    {__HI(x) = (hx&0x800fffff)|(k<<20); return x;}
        if (k <= -54)
            if (n > 50000)
		return huge*copysign(huge,x);
	    else return tiny*copysign(tiny,x);
        k += 54;
        __HI(x) = (hx&0x800fffff)|(k<<20);
        return x*twom54;
}
