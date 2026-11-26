#!/usr/bin/env python3
"""Test production image creation, child management, and list-node allocation failures."""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[2]
s = (root/'gui/Hgui.cpp').read_text()
a = s.index('int32_t HguiHandler::HandleImageView(')
b = s.index('    if ((uint32_t)cpu->ebx == SET_IMAGE)', a)
branch = s[a:b] + '    return -1;\n}\n'
widget = (root/'gui/widget.cpp').read_text()
child_management = widget[widget.index('bool Widget::AddChild('):widget.index('// Identification.')]
destructor = widget[widget.index('CompositeWidget::~CompositeWidget()'):widget.index('bool CompositeWidget::IsComposite()')]
code = r'''
#include <cstdint>
#include <cassert>
#include <cstdio>
#include <new>
#include <utils/linkedList.h>
size_t nodeAllocations=0,failNode=0;
void* operator new(size_t bytes, const std::nothrow_t&) noexcept {
    if (++nodeAllocations == failNode) return nullptr;
    return ::operator new(bytes);
}
void operator delete(void* p, const std::nothrow_t&) noexcept { ::operator delete(p); }
void* kmalloc(size_t bytes) { return ::operator new(bytes, std::nothrow); }
constexpr unsigned CREATE=1;
struct CPUState {uint32_t ebx=CREATE;};
struct WidgetData {int param0=1,param1=4,param2=8,param3=32,param4=32;};
struct ProcessControlBlock {uint32_t pid=7;} process;
ProcessControlBlock* GetCurrentProcSafe(){return &process;}
bool failResize;int destroyed=0,constructorCacheAllocations=0;
struct Widget {
    uint32_t ID=0,PID=0;Widget* parent=nullptr;
    int w,h;LinkedList<Widget*> childrenList;
    static LinkedList<Widget*>* registry;
    Widget(Widget* p,int width,int height):parent(p),w(width),h(height){if(w>0&&h>0)++constructorCacheAllocations;}
    virtual ~Widget(){
        if(registry)registry->Remove([&](Widget* p){return p==this;});
        if(parent)parent->RemoveChild(this);
        ++destroyed;
    }
    virtual bool IsComposite(){return false;}
    bool Resize(int width,int height){if(failResize)return false;w=width;h=height;return true;}
    void SetPID(uint32_t p){PID=p;}void SetID(uint32_t id){ID=id;}
    void MarkDirty(){}
    bool AddChild(Widget* child);
    bool RemoveChild(Widget* child);
};
LinkedList<Widget*>* Widget::registry;
struct CompositeWidget:Widget {
    CompositeWidget():Widget(nullptr,0,0){ID=1;PID=7;}
    ~CompositeWidget() override;
    bool IsComposite() override{return true;}
};
struct ImageView:Widget {
    ImageView(Widget* p,int,int,int w,int h,const uint32_t*,int,int):Widget(p,w,h){}
};
struct HguiHandler {
    CompositeWidget* parent;
    LinkedList<Widget*> HguiWidgets;
    Widget* FindWidgetByID(int id){return id==1?parent:nullptr;}
    uint32_t getNewID(){return 2;}
    int32_t HandleImageView(CPUState*,const WidgetData*);
};
''' + child_management + destructor + branch + r'''
int main(){
    HguiHandler handler;CPUState cpu;WidgetData data;
    handler.parent=new CompositeWidget;Widget::registry=&handler.HguiWidgets;
    Widget* existing=new Widget(nullptr,0,0);
    assert(handler.parent->AddChild(existing));
    assert(handler.HguiWidgets.TryAdd(existing));
    auto unchanged=[&]{
        assert(handler.HguiWidgets.GetSize()==1&&handler.HguiWidgets.GetFront()==existing);
        assert(handler.parent->childrenList.GetSize()==1&&handler.parent->childrenList.GetFront()==existing);
        assert(existing->parent==handler.parent);
    };
    failResize=true;
    assert(handler.HandleImageView(&cpu,&data)==-1);unchanged();
    assert(constructorCacheAllocations==0&&destroyed==1);
    failResize=false;
    // First allocation: the parent's child-list node.
    failNode=nodeAllocations+1;
    assert(handler.HandleImageView(&cpu,&data)==-1);unchanged();
    assert(destroyed==2);
    // Second allocation: registry insertion after successful child adoption.
    failNode=nodeAllocations+2;
    assert(handler.HandleImageView(&cpu,&data)==-1);unchanged();
    assert(destroyed==3);
    failNode=0;
    assert(handler.HandleImageView(&cpu,&data)==2);
    assert(handler.parent->childrenList.GetSize()==2&&handler.HguiWidgets.GetSize()==2);
    Widget* image=handler.HguiWidgets.GetFront();assert(image!=existing);
    assert(image->parent==handler.parent);
    // Duplicate attachment preserves one owner and allocates no extra node.
    size_t allocations=nodeAllocations;
    assert(!handler.parent->AddChild(image)&&nodeAllocations==allocations);
    delete handler.parent;
    assert(handler.HguiWidgets.IsEmpty()&&destroyed==6);
    assert(constructorCacheAllocations==0);
    // Both fallible insertions also preserve an empty list on failure.
    LinkedList<Widget*> empty;
    failNode=nodeAllocations+1;assert(!empty.TryAdd(nullptr)&&empty.IsEmpty());
    failNode=nodeAllocations+1;assert(!empty.TryPushBack(nullptr)&&empty.IsEmpty());
    failNode=0;assert(empty.TryPushBack(nullptr)&&empty.GetSize()==1);
    assert(empty.PopFront()==nullptr&&empty.IsEmpty());
    puts("image cache, child-node, registry-node failure and ownership regressions passed");
}
'''
with tempfile.TemporaryDirectory(prefix='hashx86-image-create-') as tmp:
    cpp=Path(tmp)/'image.cpp';cpp.write_text(code)
    shim=Path(tmp)/'core';shim.mkdir()
    (shim/'memory.h').write_text('#pragma once\n#include <cstddef>\n#include <new>\nvoid* kmalloc(size_t);\n')
    for name, flags in [('host', ['-DGUI_HOST_BUILD=1']), ('kernel-allocation', [])]:
        binary=Path(tmp)/name
        subprocess.run(['g++','-std=c++17','-O1','-g','-fsanitize=address,undefined',
                        '-I',str(tmp),'-I',str(root/'include'),*flags,
                        str(cpp),'-o',str(binary)],check=True)
        subprocess.run([str(binary)],check=True)
