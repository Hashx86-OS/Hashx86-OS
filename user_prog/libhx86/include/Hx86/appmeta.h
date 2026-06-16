#ifndef HX86_APPMETA_H
#define HX86_APPMETA_H

#include <Hx86/stdint.h>

#define HX86_APP_META_MAGIC 0x36385848
#define HX86_APP_META_VERSION 1

enum Hx86AppType {
    HX86_APP_UNKNOWN = 0,
    HX86_APP_GUI = 1,
    HX86_APP_CLI = 2,
};

struct Hx86AppMeta {
    uint32_t magic;
    uint16_t version;
    uint16_t appType;
} __attribute__((packed));

#define HX86_DECLARE_APP(appTypeValue)                                                \
    extern "C" __attribute__((used, section(".hx86meta"))) const Hx86AppMeta        \
        g_hx86_app_meta = {HX86_APP_META_MAGIC, HX86_APP_META_VERSION, (appTypeValue)}

#endif  // HX86_APPMETA_H
