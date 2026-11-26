#!/usr/bin/env python3
"""Check the production ImageView CREATE branch with fallible allocation stubs."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]
s=(root/'gui/Hgui.cpp').read_text()
a=s.index('int32_t HguiHandler::HandleImageView(')
b=s.index('    if ((uint32_t)cpu->ebx == SET_IMAGE)',a)
branch=s[a:b]+'    return -1;\n}\n'
code=r'''
#include <cstdint>
#include <cassert>
#include <cstdio>
#include <vector>
#include <algorithm>
constexpr unsigned CREATE=1;
struct CPUState {uint32_t ebx=CREATE;};
struct WidgetData {int param0=1,param1=4,param2=8,param3=32,param4=32;};
struct ProcessControlBlock {uint32_t pid=7;} process;
ProcessControlBlock* GetCurrentProcSafe(){return &process;}
bool failResize;int destroyed=0,constructorCacheAllocations=0;
struct Registry;
struct Widget {
    uint32_t ID=0,PID=0;Widget* parent=nullptr;
    int w,h;std::vector<Widget*> children;
    static Registry* registry;
    Widget(Widget* p,int width,int height):parent(p),w(width),h(height){if(w>0&&h>0)++constructorCacheAllocations;}
    virtual ~Widget();
    virtual bool IsComposite(){return false;}
    bool Resize(int width,int height){if(failResize)return false;w=width;h=height;return true;}
    void SetPID(uint32_t p){PID=p;}void SetID(uint32_t id){ID=id;}
};
struct Registry {
    std::vector<Widget*> widgets;
    void Add(Widget* p){widgets.push_back(p);}
    void Remove(Widget* p){widgets.erase(std::remove(widgets.begin(),widgets.end(),p),widgets.end());}
};
Registry* Widget::registry;
Widget::~Widget(){
    if(registry)registry->Remove(this);
    for(auto child:children){child->parent=nullptr;delete child;}
    if(parent){auto& c=parent->children;c.erase(std::remove(c.begin(),c.end(),this),c.end());}
    ++destroyed;
}
struct CompositeWidget:Widget {
    CompositeWidget():Widget(nullptr,0,0){ID=1;PID=7;}
    bool IsComposite() override{return true;}
    bool AddChild(Widget* child){
        if(std::find(children.begin(),children.end(),child)!=children.end())return false;
        children.push_back(child);child->parent=this;return true;
    }
};
struct ImageView:Widget {
    ImageView(Widget* p,int,int,int w,int h,const uint32_t*,int,int):Widget(p,w,h){}
};
struct HguiHandler {
    CompositeWidget* parent;
    Registry HguiWidgets;
    Widget* FindWidgetByID(int id){return id==1?parent:nullptr;}
    uint32_t getNewID(){return 2;}
    int32_t HandleImageView(CPUState*,const WidgetData*);
};
'''+branch+r'''
int main(){
    HguiHandler handler;CPUState cpu;WidgetData data;
    handler.parent=new CompositeWidget;Widget::registry=&handler.HguiWidgets;
    failResize=true;
    assert(handler.HandleImageView(&cpu,&data)==-1);
    assert(handler.HguiWidgets.widgets.empty()&&handler.parent->children.empty());
    assert(constructorCacheAllocations==0&&destroyed==1);
    failResize=false;
    assert(handler.HandleImageView(&cpu,&data)==2);
    assert(handler.parent->children.size()==1&&handler.HguiWidgets.widgets.size()==1);
    // Repeating the normal userspace AddChild does not duplicate ownership.
    assert(!handler.parent->AddChild(handler.parent->children[0]));
    delete handler.parent;
    assert(handler.HguiWidgets.widgets.empty()&&destroyed==3);
    assert(constructorCacheAllocations==0);
    puts("fallible ImageView creation and atomic parent ownership regressions passed");
}
'''
with tempfile.TemporaryDirectory(prefix='hashx86-image-create-') as tmp:
    cpp=Path(tmp)/'image.cpp';cpp.write_text(code);binary=Path(tmp)/'image'
    subprocess.run(['g++','-std=c++17','-O1','-g','-fsanitize=undefined',str(cpp),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
