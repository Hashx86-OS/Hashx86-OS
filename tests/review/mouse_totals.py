#!/usr/bin/env python3
"""Exercise production mouse total updates and per-thread input polling under UBSan."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]
header=(root/'include/core/drivers/mouse.h').read_text()
mouse=(root/'core/drivers/mouse.cpp').read_text()
syscalls=(root/'core/syscalls.cpp').read_text()
threads=(root/'include/core/process_types.h').read_text()
fields='\n'.join(line for line in header.splitlines() if 'uint32_t totalD' in line)
reader=header[header.index('    void GetMouseTotals('):header.index('    // Get the current button state')]
a=mouse.index('            int32_t dx =')
b=mouse.index('            eventHandler->OnMouseMove(dx, dy);',a)+len('            eventHandler->OnMouseMove(dx, dy);')
update=mouse[a:b]
a=syscalls.index('            if (MouseDriver::activeInstance)',syscalls.index('// Mouse deltas are reported per thread'))
b=syscalls.index('            if (!CopyToUser(',a)
poll=syscalls[a:b]
a=threads.index('    bool mouseSeenValid = false;')
b=threads.index('};',a)
baselines=threads[a:b]
assert 'accumDX' not in header and 'GetMouseDelta' not in header
code=r'''
#include <cstdint>
#include <cassert>
#include <cstdio>
#include <type_traits>
struct MouseEventHandler { void OnMouseMove(int32_t,int32_t){} } handler;
struct MouseDriver {
'''+fields+r'''
    static MouseDriver* activeInstance;
'''+reader+r'''
    uint8_t GetButtons(){return 0;}
    void Packet(int8_t x,int8_t y){
        uint8_t buffer[]={0,(uint8_t)x,(uint8_t)-y};
        MouseEventHandler* eventHandler=&handler;
'''+update+r'''
    }
};
MouseDriver* MouseDriver::activeInstance;
struct ThreadControlBlock {
'''+baselines+r'''
};
struct Scheduler {
    ThreadControlBlock* currentThread;
    ThreadControlBlock* GetCurrentThread(){return currentThread;}
    static Scheduler* activeInstance;
} scheduler;
Scheduler* Scheduler::activeInstance=&scheduler;
struct InputState {int32_t mouseDX=0,mouseDY=0;uint8_t mouseButtons=0;};
InputState Poll(ThreadControlBlock& thread){
    scheduler.currentThread=&thread;InputState tmp;
'''+poll+r'''
    return tmp;
}
int main(){
    static_assert(std::is_same<decltype(MouseDriver::totalDX),uint32_t>::value);
    static_assert(std::is_same<decltype(ThreadControlBlock::mouseSeenDX),uint32_t>::value);
    for(uint32_t start:{0u,0x7FFFFFFDu,0x80000002u,0xFFFFFFFDu}){
        MouseDriver mouse{};mouse.totalDX=start;mouse.totalDY=start;
        MouseDriver::activeInstance=&mouse;
        ThreadControlBlock game{},gui{};
        assert(Poll(game).mouseDX==0 && Poll(gui).mouseDY==0);
        mouse.Packet(7,-7);
        assert(mouse.totalDX==start+7u && mouse.totalDY==start-7u);
        auto first=Poll(gui);assert(first.mouseDX==7 && first.mouseDY==-7);
        mouse.Packet(-11,11);
        first=Poll(game);assert(first.mouseDX==-4 && first.mouseDY==4);
        first=Poll(gui);assert(first.mouseDX==-11 && first.mouseDY==11);
        first=Poll(game);assert(first.mouseDX==0 && first.mouseDY==0);
        ThreadControlBlock late{};
        first=Poll(late);assert(first.mouseDX==0 && first.mouseDY==0);
    }
    puts("unsigned mouse wraparound and independent polling regressions passed");
}
'''
code=code.replace('#include <type_traits>','#include <type_traits>\n#include <initializer_list>')
with tempfile.TemporaryDirectory(prefix='hashx86-mouse-wrap-') as tmp:
    cpp=Path(tmp)/'mouse.cpp';cpp.write_text(code);binary=Path(tmp)/'mouse'
    subprocess.run(['g++','-std=c++17','-O2','-g','-fsanitize=undefined',
                    '-fno-sanitize-recover=all',str(cpp),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
