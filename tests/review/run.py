#!/usr/bin/env python3
"""Native regressions using functions extracted verbatim from production sources.
Only transport, allocator, IRQ and syscall dependencies are replaced by host stubs.
Run: python3 tests/review/run.py
"""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[2]
http = (root/'user_prog/AppStore/HttpClient.cpp').read_text()
cat = (root/'user_prog/AppStore/Catalog.cpp').read_text()
util = (root/'user_prog/AppStore/StoreUtil.cpp').read_text()

def section(source, start, end):
    return source[source.index(start):source.index(end, source.index(start))]

with tempfile.TemporaryDirectory(prefix='hashx86-review-') as temp:
    tmp = Path(temp)
    def run(name, code, args=()):
        path = tmp/(name+'.cpp')
        path.write_text(code)
        out = tmp/name
        subprocess.run(['g++','-std=c++17','-O1','-g','-fsanitize=undefined',
                        '-ffunction-sections','-fdata-sections','-Wl,--gc-sections',
                        '-I',str(root/'user_prog/AppStore'),'-I',str(root/'third_party/jsmn'),
                        str(path),'-o',str(out),*args],check=True)
        subprocess.run([str(out)],check=True)
    common = '''
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <cassert>
#include <string>
#include <vector>
#include "StoreUtil.h"
'''
    common += section(util,'namespace {','bool StoreFileExists')
    # Cut filesystem helpers off the end; these are independent of parsing.
    common = common[:common.index('bool StoreReadFile')] if 'bool StoreReadFile' in common else common
    http_code = common + '''
#include "HttpClient.h"
constexpr uint32_t kMaxUrl=3072,kMaxHost=128,kMaxPath=2816,kReadBuffer=4096,kHnetChunk=4096;
struct ParsedUrl { char host[kMaxHost]; char path[kMaxPath]; uint16_t port; };
constexpr int MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY=-0x7880;
constexpr int MBEDTLS_ERR_SSL_CONN_EOF=-0x7280;
constexpr int MBEDTLS_ERR_SSL_WANT_READ=-0x6900;
constexpr int MBEDTLS_ERR_SSL_WANT_WRITE=-0x6880;
constexpr int MBEDTLS_ERR_NET_RECV_FAILED=-0x004c;
constexpr int MBEDTLS_ERR_SSL_TIMEOUT=-0x6800;
int g_ssl;
std::string wire; size_t wirePos; int wireEnd; int readCalls;
int mbedtls_ssl_read(int*, unsigned char* p,size_t n) {
    ++readCalls;
    if (wirePos==wire.size()) return wireEnd;
    if (n>wire.size()-wirePos) n=wire.size()-wirePos;
    memcpy(p,wire.data()+wirePos,n);wirePos+=n;return (int)n;
}
uint32_t ticks; uint32_t syscall_get_time_ms(){return ticks;}
void syscall_sleep(int n){ticks+=n;}
int g_socketFd=1;
namespace Hnet {
constexpr uint32_t MSG_DONTWAIT=8;
int Close(int){return 0;}
int Recv(int,unsigned char*,uint32_t,uint32_t){return -2;}
}
'''
    http_code += section(http,'void SetError(', 'bool ParseIpv4(')
    http_code += section(http,'bool ParseUrl(', 'bool ResolveHost(')
    http_code += section(http,'int TlsRecv(', 'void TlsInitCleanup(')
    http_code += section(http,'struct Reader {', 'bool SendRequest(')
    http_code += r'''
Reader feed(const std::string& s, int end=MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
    wire=s;wirePos=0;wireEnd=end;readCalls=0;return Reader{};
}
bool sink(void* p,const uint8_t* data,uint32_t len){((std::string*)p)->append((const char*)data,len);return true;}
bool headers(const std::string& s){
    Reader r=feed(s); int32_t status;bool chunked,has;uint32_t n;char loc[3072],err[256];
    return ReadFinalHeaders(&r,&status,&chunked,&has,&n,loc,sizeof(loc),err,sizeof(err));
}
int main(){
    ParsedUrl u;char out[3072],err[256];
    assert(ParseUrl("https://example.org/releases/start?version=1#details",&u));
    assert(std::string(u.path)=="/releases/start?version=1");
    assert(BuildRedirectUrl(u,"app.bin",out,sizeof(out)));
    assert(std::string(out)=="https://example.org/releases/app.bin");
    assert(BuildRedirectUrl(u,"../app.bin",out,sizeof(out)));
    assert(std::string(out)=="https://example.org/app.bin");
    assert(BuildRedirectUrl(u,"#details",out,sizeof(out)));
    assert(std::string(out)=="https://example.org/releases/start?version=1#details");
    assert(BuildRedirectUrl(u,"https://cdn.org/app.bin",out,sizeof(out)));
    assert(!BuildRedirectUrl(u,"http://cdn.org/app.bin",out,sizeof(out)));
    assert(ParseUrl("https://a?version=1#details",&u) && std::string(u.path)=="/?version=1");
    assert(!ParseUrl(("https://a?"+std::string(2815,'a')).c_str(),&u));
    assert(ParseUrl(("https://"+std::string(127,'h')+"/a").c_str(),&u));
    assert(!BuildRedirectUrl(u,("/"+std::string(3000,'a')).c_str(),out,sizeof(out)));
    assert(headers("HTTP/1.1 103 Early Hints\r\n\r\nHTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n"));
    assert(!headers("HTTP/1.1 101 Switch\r\n\r\n"));
    assert(!headers("HTTP/1.1 200 OK\r\nContent-Length: 2junk\r\n\r\n"));
    assert(!headers("HTTP/1.1 200 OK\r\nContent-Length: 2\r\nContent-Length: 3\r\n\r\n"));
    assert(headers("HTTP/1.1 200 OK\r\nContent-Length: 02\r\nContent-Length: 2 \r\n\r\n"));
    Reader r=feed("0\r\n\r\n");assert(ReadChunkedBody(&r,nullptr,nullptr,err,sizeof(err)));
    r=feed("0\r\nX-Trailer: a\r\n");assert(!ReadChunkedBody(&r,nullptr,nullptr,err,sizeof(err)));
    r=feed("0\r\n");assert(!ReadChunkedBody(&r,nullptr,nullptr,err,sizeof(err)));
    r=feed("body",0);assert(!ReadUntilEof(&r,nullptr,nullptr,err,sizeof(err)));
    r=feed("body");assert(ReadUntilEof(&r,nullptr,nullptr,err,sizeof(err)));
    r=feed("body",0);assert(ReadFixedBody(&r,4,nullptr,nullptr,err,sizeof(err)));
    r=feed("4\r\nbody\r\n0\r\n\r\n",0);assert(ReadChunkedBody(&r,nullptr,nullptr,err,sizeof(err)));
    r=feed("",0);assert(ReadResponseBody(204,&r,false,false,0,nullptr,nullptr,err,sizeof(err)));assert(readCalls==0);
    unsigned char byte; ticks=0; assert(TlsRecv(nullptr,&byte,1)==MBEDTLS_ERR_SSL_TIMEOUT);
    assert(ticks==15000 && g_socketFd==-1);
    puts("HTTP framing, URL, redirect, and receive deadline regressions passed");
}
'''
    run('http',http_code)
    catalog_code = common+'''
#include "Catalog.h"
#include "AppStoreConfig.h"
#define JSMN_STATIC
#include <jsmn.h>
'''+section(cat,'constexpr uint32_t kReleaseBodyCapacity','void DigestToHex')
    catalog_code += section(cat,'bool IsHexString(', 'bool DigestMatches(')
    catalog_code += section(cat,'bool TokenEquals(', 'bool ParseReleaseDocument(')
    catalog_code += section(cat,'const ReleaseAsset* FindReleaseAsset(', '}  // namespace')
    catalog_code += r'''
bool parse(const std::string& apps){
    std::string body="{\"schema\":1,\"apps\":"+apps+"}";
    memcpy(g_catalogBody,body.data(),body.size());g_catalogLength=body.size();
    extern Catalog parsed;char err[256];return ParseCatalog(&parsed,err,sizeof(err));
}
Catalog parsed;
int main(){
    char out[12];
    auto copy=[&](const char* text,unsigned n=12){jsmntok_t t={JSMN_STRING,0,(int)strlen(text),0};return TokenCopy(text,&t,out,n);};
    assert(copy("\\uD83D\\uDE00",5));assert(std::string(out)=="\xf0\x9f\x98\x80");
    assert(!copy("\\uD83D\\uDE00",4));assert(!copy("\\uD83D"));assert(!copy("\\uDE00"));
    assert(!copy("abc\\u0000def")); assert(copy("\\u0041") && out[0]=='A');
    const std::string valid=R"({"id":"app","name":"App","version":"1","repo":"owner/repo","asset":"app.bin","binary":"app.bin"})";
    assert(parse("["+valid+"]"));
    std::string bad=valid;bad.replace(bad.find("\"asset\":\"app.bin\""),17,"\"asset\":\"\"");assert(!parse("["+bad+"]"));
    bad=valid;bad.replace(bad.find("\"version\":\"1\""),13,"\"version\":1");assert(!parse("["+bad+"]"));
    bad=valid;bad.replace(bad.find("\"id\":\"app\""),10,"\"id\":\"APP\"");assert(!parse("["+valid+","+bad+"]"));
    bad=valid;bad.replace(bad.find("\"id\":\"app\""),10,"\"id\":\"other\"");assert(parse("["+valid+","+bad+"]"));
    std::string url=R"({"id":"url","name":"URL","version":"1","url":"https://host/app.bin?next=/wrong.bin#foo","size":1,"sha256":")"+std::string(64,'a')+"\"}";
    assert(parse("["+url+"]"));assert(std::string(parsed.apps[0].binary)=="app.bin");
    url=R"({"id":"url","name":"URL","version":"1","url":"https://host/)"+std::string(200,'a')+R"(.bin","size":1,"sha256":")"+std::string(64,'a')+"\"}";
    assert(!parse("["+url+"]"));
    puts("catalog Unicode and field-validation regressions passed");
}
'''
    run('catalog',catalog_code)
    division = (root/'user_prog/libmbedtls/64bit.cpp').read_text()
    run('division',common+division+r'''
int main(){
    const int64_t m=INT64_MIN;
    assert(__moddi3(m,1)==0);assert(__moddi3(m,-1)==0);
    assert(__divdi3(m,1)==m);assert(__divdi3(-7,3)==-2);assert(__moddi3(-7,3)==-1);
    assert(__divdi3(7,-3)==-2);assert(__moddi3(7,-3)==1);
    assert(__udivdi3(UINT64_MAX,UINT64_MAX)==1);
    puts("64-bit division boundary regressions passed");
}
''')
    port = (root/'user_prog/libmbedtls/mbedtls_port.cpp').read_text()
    run('clock',common+'''
#include <ctime>
using mbedtls_time_t = long long;
uint32_t clockSeconds;
uint32_t syscall_time(uint32_t*) { return clockSeconds; }
'''+section(port,'mbedtls_time_t TlsTime(', '}  // namespace')+
        section(port,'struct tm* mbedtls_platform_gmtime_r(', 'void Hashx86MbedtlsPlatformInit(')+r'''
int main(){
    clockSeconds=2147483648u;
    mbedtls_time_t seconds=TlsTime(nullptr);tm utc{};
    assert(seconds==2147483648LL);
    assert(mbedtls_platform_gmtime_r(&seconds,&utc));
    assert(utc.tm_year==138 && utc.tm_mon==0 && utc.tm_mday==19);
    clockSeconds=0xFFFFFFFFu;seconds=TlsTime(nullptr);
    assert(seconds==-1 && !mbedtls_platform_gmtime_r(&seconds,&utc));
    puts("post-2038 clock and failure-sentinel regressions passed");
}
''')
    image = (root/'gui/imageview.cpp').read_text()
    run('image',common+'''
#include <cstdlib>
#include <new>
bool failAllocation;
void* operator new[](size_t size) { return failAllocation ? nullptr : malloc(size); }
void operator delete[](void* p) noexcept { free(p); }
void operator delete[](void* p, size_t) noexcept { free(p); }
struct ImageView {
    uint32_t* pixels=nullptr;int32_t srcW=0,srcH=0;
    void MarkDirty(){}
    bool SetImage(const uint32_t*,int32_t,int32_t);
};
'''+section(image,'bool ImageView::SetImage(', 'void ImageView::RedrawToCache(')+r'''
int main(){
    ImageView image;uint32_t first[]={1,2,3,4},second[]={9};
    assert(image.SetImage(first,2,2));auto old=image.pixels;
    failAllocation=true;
    assert(!image.SetImage(second,1,1));assert(image.pixels==old && image.pixels[3]==4);
    assert(!image.SetImage(second,-1,1));assert(image.pixels==old);
    failAllocation=false;
    assert(image.SetImage(second,1,1) && image.pixels[0]==9);
    assert(image.SetImage(nullptr,0,0) && image.pixels==nullptr);
    puts("image replacement allocation-failure regressions passed");
}
''',('-fcheck-new','-fno-exceptions'))
    rng=(root/'core/random.cpp').read_text()
    rng=rng.replace('#include <core/random.h>','').replace('#include <core/Iguard.h>','')
    run('random',common+'''
struct InterruptGuard {};
uint64_t timerTicks;
bool RandomInitialize();
'''+rng+r'''
int main(){
    uint32_t input[16]={0x61707865,0x3320646e,0x79622d32,0x6b206574},out[16];
    Block(input,out);
    const uint8_t expected[]={0x76,0xb8,0xe0,0xad,0xa0,0xf1,0x3d,0x90,0x40,0x5d,0x6a,0xe5,0x53,0x86,0xbd,0x28};
    assert(!memcmp(out,expected,sizeof(expected)));
    assert(RandomInitialize());
    uint8_t a[64],b[64];assert(RandomBytes(a,64) && RandomBytes(b,64));assert(memcmp(a,b,64));
    uint32_t first=RandomTcpIsn(1,2,3,4),second=RandomTcpIsn(1,2,3,4);
    assert(first!=second);
    puts("ChaCha20 vector and hardware-seeded RNG regressions passed");
}
''')
    cache_code = common+'''
struct mbedtls_x509_time { int year; };
struct mbedtls_ssl_session { int ticket; };
struct mbedtls_x509_crt { mbedtls_x509_time valid_to; };
int g_ssl;
int year=2026;
mbedtls_x509_crt certificate{{2027}};
const mbedtls_x509_crt* peer=&certificate;
int mbedtls_x509_time_is_past(const mbedtls_x509_time* time){return time->year < year;}
void mbedtls_ssl_session_free(mbedtls_ssl_session* s){s->ticket=0;}
void mbedtls_ssl_session_init(mbedtls_ssl_session* s){s->ticket=0;}
int mbedtls_ssl_get_session(int*,mbedtls_ssl_session* s){s->ticket=1;return 0;}
const mbedtls_x509_crt* mbedtls_ssl_get_peer_cert(int*){return peer;}
constexpr uint32_t kMaxHost=128;
'''+section(http,'constexpr uint32_t kMaxTlsSessions', 'void SetError(')+r'''
int main(){
    RememberTlsSession("host");assert(FindTlsSession("host"));
    peer=nullptr;RememberTlsSession("host");
    assert(FindTlsSession("host")->notAfter.year==2027);
    year=2028;assert(!FindTlsSession("host"));
    puts("TLS resumed-session certificate-expiry regressions passed");
}
'''
    run('tls-cache',cache_code)
    prog=(root/'user_prog/AppStore/prog.cpp').read_text()
    run('promotion',common+r'''
#include <set>
std::set<std::string> files;
bool failPromotion,failRestore,failed;
const char* binaryPath="/Apps/app/app.bin";
const char* tempPath="/Apps/app/app.tmp";
bool StoreFileExists(const char* path,uint32_t*){return files.count(path)!=0;}
int syscall_unlink(const char* path){return files.erase(path)?0:-1;}
int syscall_rename(const char* from,const char* to){
    if ((failPromotion && std::string(from)==tempPath) ||
        (failRestore && std::string(from)==std::string(binaryPath)+".backup")) return -1;
    if (!files.count(from) || files.count(to)) return -1;
    files.erase(from);files.insert(to);return 0;
}
void WorkerFail(const char*){failed=true;}
void promote(){
'''+section(prog,'    char backupPath[192];','    g_busy = false;')+r'''
}
int main(){
    files={binaryPath,tempPath};promote();assert(files==std::set<std::string>{binaryPath});
    files={binaryPath,tempPath};failPromotion=true;promote();
    assert(failed && files==std::set<std::string>{binaryPath});
    files={binaryPath,tempPath};failRestore=true;promote();
    assert(files.count(std::string(binaryPath)+".backup") && files.count(tempPath));
    puts("binary promotion and rollback regressions passed");
}
''')
    fs=(root/'core/filesystem/FatFs/FatFsWrapper.cpp').read_text()
    run('remove',common+'''
struct InterruptGuard {};
#define KDBG1(...)
#define KDBG2(...)
constexpr int FR_OK=0,AM_DIR=0x10;
using FRESULT=int;
struct FILINFO { int fattrib; };
int attributes,unlinks;
int f_stat(const char*,FILINFO* p){p->fattrib=attributes;return FR_OK;}
int f_unlink(const char*){++unlinks;return FR_OK;}
struct FatFsWrapper {
    bool PathToFatFs(const char*,char*,size_t){return true;}
    int32_t Remove(const char*,bool);
};
'''+section(fs,'int32_t FatFsWrapper::Remove(', 'int32_t FatFsWrapper::Rename(')+r'''
int main(){
    FatFsWrapper fs;
    attributes=AM_DIR;assert(fs.Remove("dir",false)==-1 && unlinks==0);
    assert(fs.Remove("dir",true)==0 && unlinks==1);
    attributes=0;assert(fs.Remove("file",true)==-1 && unlinks==1);
    assert(fs.Remove("file",false)==0 && unlinks==2);
    puts("typed filesystem removal regressions passed");
}
''')
    scheduler=(root/'core/scheduler.cpp').read_text()
    socket=(root/'core/net/net_socket.cpp').read_text()
    run('lifetime',common+'''
#include <algorithm>
#include <cstdlib>
#define KDBG1(...)
struct InterruptGuard {};
struct ThreadControlBlock;
struct List {
    std::vector<ThreadControlBlock*> v;
    template<class F> void Remove(F f) { v.erase(std::remove_if(v.begin(),v.end(),f),v.end()); }
    void PushBack(ThreadControlBlock* t){v.push_back(t);}
};
struct ProcessControlBlock { bool killPending=false;uint32_t* page_directory=nullptr;List threads; };
constexpr int THREAD_STATE_TERMINATED=5;
struct ThreadControlBlock {
    uint32_t networkCallDepth=0;bool terminatePending=false;int state=1,tid=1;
    ProcessControlBlock* parent=nullptr;uint32_t stackSlotIdx=UINT32_MAX;void* stack=nullptr;
};
struct Offsets { void Add(uint32_t){} } g_freeStackOffsets;
void FreeUserStackSlot(uint32_t*,uint32_t){}
int freedStacks;
void kstack_free(void* p){++freedStacks;free(p);}
struct Scheduler {
    static Scheduler* activeInstance;
    ThreadControlBlock* currentThread=nullptr;List readyQueue,blockedQueue,pendingReclaims;
    void TerminateThread(ThreadControlBlock*);
};
Scheduler* Scheduler::activeInstance;
'''+section(scheduler,'void Scheduler::TerminateThread(', 'bool Scheduler::ExitCurrentThread(')+
        section(socket,'class NetworkCallGuard {', 'constexpr int kDeferredCloseCapacity')+r'''
int main(){
    Scheduler sched;Scheduler::activeInstance=&sched;ProcessControlBlock process;
    auto thread=new ThreadControlBlock;thread->parent=&process;thread->stack=malloc(64);
    process.threads.PushBack(thread);sched.currentThread=thread;
    {
        NetworkCallGuard pin;
        sched.TerminateThread(thread);
        assert(thread->terminatePending && thread->networkCallDepth==1);
        assert(thread->stack && freedStacks==0 && process.threads.v.size()==1);
        // A simulated in-flight lwIP completion can still write the saved stack.
        ((uint8_t*)thread->stack)[0]=42;
    }
    assert(!sched.currentThread && sched.pendingReclaims.v.size()==1);
    assert(freedStacks==0 && process.threads.v.empty());
    // After switching stacks the scheduler can safely reclaim the allocation.
    kstack_free(thread->stack);delete thread;
    auto second=new ThreadControlBlock;second->parent=&process;second->stack=malloc(64);
    sched.currentThread=second;process.threads.PushBack(second);
    {
        NetworkCallGuard pin;process.killPending=true;
        assert(CancelRequested());
    }
    assert(!sched.currentThread && sched.pendingReclaims.v.size()==2);
    kstack_free(second->stack);delete second;
    puts("network request stack-lifetime regressions passed");
}
''')
    rtl=(root/'core/drivers/rtl8139.cpp').read_text()
    run('tx',common+'''
constexpr unsigned kTxDescriptors=4;
constexpr uint16_t kRegTsd0=0x10,kRegTcr=0x40;
constexpr uint32_t kTsdTxOk=0x8000,kTcrConfig=0x3000680;
constexpr uint32_t kTsdTxUnderrun=1u<<14,kTsdTxAborted=1u<<30,kTcrClearAbort=1;
uint32_t status[4];int abortClears;
uint32_t inl(uint16_t port){return status[(port-kRegTsd0)/4];}
void outl(uint16_t port,uint32_t value){assert(port==kRegTcr && (value&1));++abortClears;}
struct Rtl8139Device { bool txBusy[4];uint16_t ioBase=0;void ReclaimTransmitDescriptors(); };
'''+section(rtl,'void Rtl8139Device::ReclaimTransmitDescriptors()', 'bool Rtl8139Device::SendFrame(')+r'''
int main(){
    Rtl8139Device tx;
    for(int repeat=0;repeat<10;repeat++){
        for(int i=0;i<4;i++){tx.txBusy[i]=true;status[i]=(i&1)?(1u<<30):(1u<<14);}
        tx.ReclaimTransmitDescriptors();for(bool busy:tx.txBusy)assert(!busy);
    }
    assert(abortClears==20);
    tx.txBusy[0]=true;status[0]=0;tx.ReclaimTransmitDescriptors();assert(tx.txBusy[0]);
    status[0]=kTsdTxOk;tx.ReclaimTransmitDescriptors();assert(!tx.txBusy[0]);
    puts("RTL8139 terminal transmit-error regressions passed");
}
''')
    rng_fail = rng.replace(section(rng,'bool HardwareWord(', '}  // namespace'),
                           'bool HardwareWord(uint32_t*) { return false; }\n')
    run('entropy-failure',common+'''
struct InterruptGuard {};
uint64_t timerTicks;
bool RandomInitialize();
'''+rng_fail+r'''
int main(){
    uint32_t bytes=0xdeadbeef;
    assert(!RandomInitialize());assert(!RandomBytes(&bytes,sizeof(bytes)));
    assert(bytes==0xdeadbeef);
    puts("kernel RNG missing-entropy failure regression passed");
}
''')
    run('tls-entropy-failure',common+'''
constexpr int MBEDTLS_ERR_ENTROPY_SOURCE_FAILED=-60;
int32_t syscall_random(void*,uint32_t){return -1;}
'''+section(port,'int mbedtls_hardware_poll(', '/**\n * mbedtls_platform_gmtime_r')+r'''
int main(){
    unsigned char bytes[32];size_t written=99;
    assert(mbedtls_hardware_poll(nullptr,bytes,sizeof(bytes),&written)!=0 && written==0);
    puts("TLS entropy failure propagation regression passed");
}
''')
    init_code = common+'''
#include <cstdlib>
struct Context { int allocated; };
Context g_entropy{},g_ctrDrbg{},g_ca{},g_sslConfig{};
bool g_tlsReady=false,failSeed=true;int allocations=0;
const char* kCaPath="test.pem";
void Hashx86MbedtlsPlatformInit(){}
void init(Context* c){c->allocated=0;}
void release(Context* c){if(c->allocated)--allocations;c->allocated=0;}
void mbedtls_entropy_init(Context* c){init(c);}
void mbedtls_ctr_drbg_init(Context* c){init(c);}
void mbedtls_x509_crt_init(Context* c){init(c);}
void mbedtls_ssl_config_init(Context* c){init(c);}
void mbedtls_entropy_free(Context* c){release(c);}
void mbedtls_ctr_drbg_free(Context* c){release(c);}
void mbedtls_x509_crt_free(Context* c){release(c);}
void mbedtls_ssl_config_free(Context* c){release(c);}
int mbedtls_entropy_func(...){return 0;}
int mbedtls_ctr_drbg_random(...){return 0;}
int mbedtls_ctr_drbg_seed(Context* c,...){c->allocated=1;++allocations;return failSeed?-1:0;}
int mbedtls_x509_crt_parse(Context* c,...){c->allocated=1;++allocations;return 0;}
int mbedtls_ssl_config_defaults(Context* c,...){c->allocated=1;++allocations;return 0;}
void mbedtls_ssl_conf_authmode(...){}
void mbedtls_ssl_conf_ca_chain(...){}
void mbedtls_ssl_conf_rng(...){}
void mbedtls_ssl_conf_alpn_protocols(...){}
constexpr int MBEDTLS_SSL_IS_CLIENT=0,MBEDTLS_SSL_TRANSPORT_STREAM=0,MBEDTLS_SSL_PRESET_DEFAULT=0,MBEDTLS_SSL_VERIFY_REQUIRED=0;
bool StoreReadFile(const char*,uint8_t** data,uint32_t* size){*data=(uint8_t*)malloc(2);*size=1;return true;}
void kfree(void* p){free(p);}
'''+section(http,'void SetError(', 'bool ContainsCase(')+section(http,'void TlsInitCleanup(', 'void TlsClose()')+r'''
int main(){
    char error[256];assert(!TlsEnsureReady(error,sizeof(error)) && allocations==0);
    failSeed=false;assert(TlsEnsureReady(error,sizeof(error)) && allocations==3);
    assert(TlsEnsureReady(error,sizeof(error)) && allocations==3);
    TlsInitCleanup();assert(allocations==0);
    puts("TLS initialization cleanup and retry regressions passed");
}
'''
    run('tls-init',init_code)
