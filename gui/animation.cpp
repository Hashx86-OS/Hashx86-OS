#define KDBG_COMPONENT "GUI:ANIM"
#include <gui/animation.h>
#include <gui/widget.h>

AnimationManager* AnimationManager::activeInstance = nullptr;

AnimationManager::AnimationManager() {
    activeInstance = this;
    nextAnimId = 1;
}

AnimationManager::~AnimationManager() {
    activeAnims.ForEach([&](Animation* a) { delete a; });
    activeAnims.Clear();
    pendingAnims.ForEach([&](Animation* a) { delete a; });
    pendingAnims.Clear();
}

static uint32_t CalcProgress(uint32_t elapsed, uint32_t duration) {
    if (elapsed >= duration) return 65536;
    // Compute elapsed * 65536 / duration without 64-bit math.
    // Since elapsed < duration, result < 65536 fits in 16 bits.
    // (elapsed << 16) overflows 32-bit if elapsed > 0xFFFF,
    // but for animations < ~65s this is fine. For longer ones,
    // scale both down keeping the ratio approximately correct.
    if (elapsed <= 0xFFFF) {
        return (elapsed << 16) / duration;
    }
    // Long animation fallback: shift right by 8 (loses <0.4% precision)
    return ((elapsed >> 8) << 16) / (duration >> 8);
}

int32_t AnimationManager::ApplyEasing(EaseType type, uint32_t elapsed,
                                       uint32_t duration, int32_t from, int32_t to) {
    uint32_t t = CalcProgress(elapsed, duration);
    uint32_t eased;

    switch (type) {
        case EASE_LINEAR:
            eased = t;
            break;

        case EASE_IN_QUAD:
            eased = (uint32_t)((uint64_t)t * t / 65536);
            break;

        case EASE_OUT_QUAD: {
            uint32_t inv = 65536 - t;
            eased = 65536 - (uint32_t)((uint64_t)inv * inv / 65536);
            break;
        }

        case EASE_IN_OUT_QUAD:
            if (t < 32768) {
                eased = (uint32_t)((uint64_t)t * t / 32768);
            } else {
                uint32_t inv = 65536 - t;
                eased = 65536 - (uint32_t)((uint64_t)inv * inv / 32768);
            }
            break;

        case EASE_OUT_CUBIC: {
            uint32_t inv = 65536 - t;
            uint64_t inv2 = (uint64_t)inv * inv / 65536;
            eased = 65536 - (uint32_t)(inv2 * inv / 65536);
            break;
        }

        default:
            eased = t;
            break;
    }

    int64_t range = (int64_t)to - (int64_t)from;
    return from + (int32_t)(range * eased / 65536);
}

void AnimationManager::ApplyValue(Animation* anim, int32_t value) {
    switch (anim->property) {
        case ANIM_PROP_X: anim->widget->x = value; break;
        case ANIM_PROP_Y: anim->widget->y = value; break;
        case ANIM_PROP_W: anim->widget->w = value; break;
        case ANIM_PROP_H: anim->widget->h = value; break;
        case ANIM_PROP_OPACITY:
            anim->widget->SetAlpha((uint8_t)value);
            break;
        case ANIM_PROP_COLOR:
            anim->widget->SetColorIndex((uint32_t)value);
            break;
    }
    anim->widget->MarkDirty();
}

void AnimationManager::Tick(uint64_t nowMs) {
    LinkedList<Animation*> toActivate;
    pendingAnims.ForEach([&](Animation* anim) {
        if (nowMs >= anim->startTime + anim->delayMs) {
            anim->startTime = nowMs;
            toActivate.PushBack(anim);
        }
    });
    toActivate.ForEach([&](Animation* a) {
        activeAnims.PushBack(a);
        pendingAnims.Remove([&](Animation* p) { return p == a; });
    });

    LinkedList<Animation*> completed;
    activeAnims.ForEach([&](Animation* anim) {
        if (!anim->widget) { completed.PushBack(anim); return; }

        uint32_t elapsed = (uint32_t)(nowMs - anim->startTime);
        int32_t value = ApplyEasing(anim->easing, elapsed,
                                     anim->durationMs, anim->fromValue, anim->toValue);
        ApplyValue(anim, value);

        if (elapsed >= anim->durationMs) {
            ApplyValue(anim, anim->toValue);
            completed.PushBack(anim);
        }
    });

    completed.ForEach([&](Animation* anim) {
        activeAnims.Remove([&](Animation* a) { return a == anim; });
        if (anim->onComplete) {
            anim->onComplete(anim->widget, anim->completionCtx);
        }
        if (anim->next) {
            anim->next->startTime = anim->startTime + anim->durationMs;
            activeAnims.PushBack(anim->next);
            anim->next = nullptr;
        }
        delete anim;
    });
}

uint32_t AnimationManager::Animate(Widget* widget, AnimProperty prop,
                                    int32_t from, int32_t to,
                                    uint32_t durationMs, EaseType easing,
                                    uint32_t delayMs) {
    Animation* anim = new Animation();
    if (!anim) return 0;
    anim->widget = widget;
    anim->property = prop;
    anim->fromValue = from;
    anim->toValue = to;
    anim->durationMs = durationMs;
    anim->startTime = 0;
    anim->easing = easing;
    anim->delayMs = delayMs;
    anim->next = nullptr;
    anim->onComplete = nullptr;
    anim->completionCtx = nullptr;

    uint32_t id = nextAnimId++;

    if (delayMs > 0) {
        pendingAnims.PushBack(anim);
    } else {
        activeAnims.PushBack(anim);
    }

    return id;
}

void AnimationManager::CancelAll(Widget* widget) {
    activeAnims.Remove([&](Animation* a) {
        if (a->widget == widget) { delete a; return true; }
        return false;
    });
    pendingAnims.Remove([&](Animation* a) {
        if (a->widget == widget) { delete a; return true; }
        return false;
    });
}

void AnimationManager::Cancel(uint32_t animId) {
    (void)animId;
}

uint32_t AnimationManager::AnimateAfter(uint32_t afterAnimId,
                                         Widget* widget, AnimProperty prop,
                                         int32_t from, int32_t to,
                                         uint32_t durationMs, EaseType easing) {
    (void)afterAnimId;
    return Animate(widget, prop, from, to, durationMs, easing);
}
