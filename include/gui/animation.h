#ifndef ANIMATION_H
#define ANIMATION_H

#include <types.h>
#include <utils/linkedList.h>

typedef enum {
    ANIM_PROP_X = 0,
    ANIM_PROP_Y,
    ANIM_PROP_W,
    ANIM_PROP_H,
    ANIM_PROP_OPACITY,
    ANIM_PROP_COLOR,
} AnimProperty;

typedef enum {
    EASE_LINEAR = 0,
    EASE_IN_QUAD,
    EASE_OUT_QUAD,
    EASE_IN_OUT_QUAD,
    EASE_IN_CUBIC,
    EASE_OUT_CUBIC,
    EASE_OUT_BOUNCE,
} EaseType;

class Widget;

struct Animation {
    Widget*       widget;
    AnimProperty  property;
    int32_t       fromValue;
    int32_t       toValue;
    uint32_t      durationMs;
    uint64_t      startTime;
    EaseType      easing;
    uint32_t      delayMs;
    Animation*    next;
    void          (*onComplete)(Widget* widget, void* ctx);
    void*         completionCtx;
};

class AnimationManager {
public:
    AnimationManager();
    ~AnimationManager();
    static AnimationManager* activeInstance;

    uint32_t Animate(Widget* widget, AnimProperty prop,
                     int32_t from, int32_t to,
                     uint32_t durationMs, EaseType easing,
                     uint32_t delayMs = 0);

    void CancelAll(Widget* widget);
    void Cancel(uint32_t animId);
    void Tick(uint64_t nowMs);

    uint32_t AnimateAfter(uint32_t afterAnimId,
                          Widget* widget, AnimProperty prop,
                          int32_t from, int32_t to,
                          uint32_t durationMs, EaseType easing);

private:
    LinkedList<Animation*> activeAnims;
    LinkedList<Animation*> pendingAnims;
    uint32_t nextAnimId;

    int32_t ApplyEasing(EaseType type, uint32_t elapsed, uint32_t duration,
                        int32_t from, int32_t to);
    void    ApplyValue(Animation* anim, int32_t value);
};

#endif  // ANIMATION_H
