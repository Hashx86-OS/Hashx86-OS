#ifndef HANIMATION_H
#define HANIMATION_H

#include <Hx86/Hgui/widget.h>

typedef enum {
    ANIM_X = 0,
    ANIM_Y,
    ANIM_W,
    ANIM_H,
    ANIM_OPACITY,
    ANIM_COLOR,
} AnimTarget;

typedef enum {
    LINEAR = 0,
    EASE_IN,
    EASE_OUT,
    EASE_IN_OUT,
    EASE_CUBIC_OUT,
    EASE_BOUNCE,
} Easing;

class Animator {
public:
    static uint32_t animate(Widget* widget, AnimTarget prop,
                            int32_t from, int32_t to,
                            uint32_t durationMs,
                            Easing easing = LINEAR);

    static uint32_t chain(uint32_t afterAnimId,
                          Widget* widget, AnimTarget prop,
                          int32_t from, int32_t to,
                          uint32_t durationMs,
                          Easing easing = LINEAR);

    static void cancel(uint32_t animId);
    static void cancelAll(Widget* widget);
};

#endif  // HANIMATION_H
