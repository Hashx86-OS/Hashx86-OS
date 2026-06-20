#include <Hx86/Hgui/animation.h>

uint32_t Animator::animate(Widget* widget, AnimTarget prop,
                            int32_t from, int32_t to,
                            uint32_t durationMs,
                            Easing easing) {
    if (!widget) return 0;
    WidgetData data = {widget->ID, (int32_t)prop, from, to,
                       (uint32_t)((durationMs << 16) | (uint32_t)easing)};
    return HguiAPI(ANIMATION, ANIM_START_EX, (void*)&data);
}

uint32_t Animator::chain(uint32_t afterAnimId,
                          Widget* widget, AnimTarget prop,
                          int32_t from, int32_t to,
                          uint32_t durationMs,
                          Easing easing) {
    if (!widget) return 0;
    WidgetData data = {afterAnimId, (int32_t)prop, from, to,
                       (uint32_t)((durationMs << 16) | (uint32_t)easing)};
    return HguiAPI(ANIMATION, ANIM_CHAIN, (void*)&data);
}

void Animator::cancel(uint32_t animId) {
    WidgetData data = {animId};
    HguiAPI(ANIMATION, ANIM_CANCEL, (void*)&data);
}

void Animator::cancelAll(Widget* widget) {
    if (!widget) return;
    WidgetData data = {widget->ID};
    HguiAPI(ANIMATION, ANIM_CANCEL_ALL, (void*)&data);
}
