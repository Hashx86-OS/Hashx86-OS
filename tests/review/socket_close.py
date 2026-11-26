#!/usr/bin/env python3
"""Exercise production receive/close code with two host threads and a blocking BIO stub."""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[2]
source = (root/'core/net/net_socket.cpp').read_text()
def section(start, end):
    return source[source.index(start):source.index(end, source.index(start))]
code = r'''
#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <cassert>
#include <cstdio>
#include <cerrno>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <vector>
#include <cstring>
std::recursive_mutex irq;
struct InterruptGuard { std::lock_guard<std::recursive_mutex> lock{irq}; };
constexpr unsigned FD_MIN=3,FD_MAX=16,MEMP_NUM_NETCONN=8,kMaxStreamChunk=4096,MSG_DONTWAIT=8;
struct ProcessControlBlock {
    int socketTable[FD_MAX];void* fdTable[FD_MAX]{};bool killPending=false;
    ProcessControlBlock(){for(int& fd:socketTable)fd=-1;}
};
struct ThreadControlBlock { int networkCallDepth=0;bool terminatePending=false;ProcessControlBlock* parent=nullptr; };
struct Scheduler {
    static Scheduler* activeInstance;
    static thread_local ThreadControlBlock* currentThread;
    void TerminateThread(ThreadControlBlock*){assert(false);}
};
Scheduler scheduler;
Scheduler* Scheduler::activeInstance=&scheduler;
thread_local ThreadControlBlock* Scheduler::currentThread;
void* kmalloc(size_t n){return malloc(n);}void kfree(void* p){free(p);}
bool CopyToUser(ProcessControlBlock*,void*,const void* data,size_t n){return n==2 && !memcmp(data,"ok",2);}
std::mutex gate;
std::condition_variable cv;
bool entered=false,released=false,blocking=true;
int receives=0;std::vector<int> closed;
int lwip_recv(int fd,void* buffer,uint32_t,uint32_t){
    ++receives;
    if (!blocking){memcpy(buffer,"ok",2);return 2;}
    assert(fd==0);
    std::unique_lock<std::mutex> lock(gate);
    entered=true;cv.notify_all();cv.wait(lock,[]{return released;});
    errno=EWOULDBLOCK;return -1;
}
int lwip_close(int fd){closed.push_back(fd);return 0;}
'''
code += section('class NetworkCallGuard {','/**\n * ReadSockaddr')
code += section('int32_t SocketRecv(', '/**\n * SocketShutdown')
code += section('void QueueDeferredClose(SocketState* state) {', '}  // namespace')
code += section('int32_t NetCloseSocketFd(', 'void NetCloseProcessSockets(')
code += source[source.index('void NetCloseProcessSockets('):]
code += r'''
int main(){
    for(int iteration=0;iteration<100;iteration++){
        ProcessControlBlock process;ThreadControlBlock closer;closer.parent=&process;
        Scheduler::currentThread=&closer;
        int fd=AllocateSocketFd(&process,0);assert(fd==3);
        uint32_t args[]={uint32_t(fd),0,4,0};
        entered=released=false;blocking=true;receives=0;closed.clear();
        std::thread receiver([&]{
            ThreadControlBlock reader;reader.parent=&process;Scheduler::currentThread=&reader;
            assert(SocketRecv(&process,args)<0);
        });
        {std::unique_lock<std::mutex> lock(gate);cv.wait(lock,[]{return entered;});}
        assert(NetCloseSocketFd(&process,fd)==0);
        NetReapClosedSockets();assert(closed.empty());
        // The user fd can be reused without changing the in-flight reference.
        assert(AllocateSocketFd(&process,1)==fd);
        {std::lock_guard<std::mutex> lock(gate);released=true;}cv.notify_all();receiver.join();
        assert(receives==1);NetReapClosedSockets();assert(closed==std::vector<int>{0});
        blocking=false;assert(SocketRecv(&process,args)==2);
        NetCloseProcessSockets(&process);NetReapClosedSockets();
        assert((closed==std::vector<int>{0,1}));
        NetReapClosedSockets();assert(closed.size()==2);
    }
    puts("two-thread receive/close and descriptor-reuse regressions passed (100 iterations)");
}
'''
with tempfile.TemporaryDirectory(prefix='hashx86-close-') as tmp:
    cpp=Path(tmp)/'close.cpp';cpp.write_text(code);binary=Path(tmp)/'close'
    subprocess.run(['g++','-std=c++17','-O1','-g','-fsanitize=undefined',
                    '-pthread',str(cpp),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True,timeout=30)
