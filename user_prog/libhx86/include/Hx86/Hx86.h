#ifndef Hx86_H
#define Hx86_H

#include <Hx86/Hgui/eventHandler.h>
#include <Hx86/Hsyscalls/syscalls.h>
#include <Hx86/appmeta.h>
#include <Hx86/debug.h>
#include <Hx86/globals.h>
#include <Hx86/memory.h>

void init_sys(void* arg);
bool init_cli();
bool cli_append_output(const char* text);
void syscall_sleep(uint32_t ms);

#endif  // Hx86_H
