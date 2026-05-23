/**
 * @file        Hgui.cpp
 * @brief       Hgui Handler (part of #x86 GUI Framework)
 *
 * @date        11/02/2026
 * @version     1.0.0
 */

#define KDBG_COMPONENT "GUI"
#include <core/globals.h>
#include <gui/Hgui.h>

namespace {
constexpr uint32_t USER_LOWER_BOUND = 0x10000000;
constexpr size_t MAX_USER_TEXT = 256;

bool IsUserRange(ProcessControlBlock* proc, uint32_t addr, size_t size) {
    if (!proc || !g_paging || size == 0) return false;
    if (addr < USER_LOWER_BOUND) return false;
    uint32_t end = addr + (uint32_t)size - 1;
    if (end < addr) return false;

    uint32_t start_page = addr & ~(PAGE_SIZE - 1);
    uint32_t end_page = end & ~(PAGE_SIZE - 1);
    for (uint32_t page = start_page; page <= end_page; page += PAGE_SIZE) {
        if (g_paging->GetPhysicalAddress(proc->page_directory, page) == 0) {
            return false;
        }
    }
    return true;
}

bool CopyFromUser(ProcessControlBlock* proc, void* dst, const void* src_user, size_t size) {
    if (!dst || !src_user || size == 0) return false;
    uint32_t user_addr = (uint32_t)src_user;
    if (!IsUserRange(proc, user_addr, size)) return false;

    uint8_t* out = (uint8_t*)dst;
    size_t remaining = size;
    while (remaining > 0) {
        uint32_t phys = g_paging->GetPhysicalAddress(proc->page_directory, user_addr);
        if (!phys) return false;
        uint32_t offset = user_addr & (PAGE_SIZE - 1);
        uint32_t chunk = PAGE_SIZE - offset;
        if (chunk > remaining) chunk = (uint32_t)remaining;
        memcpy(out, (void*)phys, chunk);
        out += chunk;
        user_addr += chunk;
        remaining -= chunk;
    }
    return true;
}

bool CopyUserString(ProcessControlBlock* proc, const char* src_user, char* dst, size_t dst_size) {
    if (!dst || dst_size == 0) return false;
    if (!src_user) {
        dst[0] = '\0';
        return true;
    }

    uint32_t user_addr = (uint32_t)src_user;
    if (user_addr < USER_LOWER_BOUND) {
        dst[0] = '\0';
        return false;
    }

    size_t i = 0;
    while (i + 1 < dst_size) {
        uint32_t phys = g_paging->GetPhysicalAddress(proc->page_directory, user_addr);
        if (!phys) {
            dst[0] = '\0';
            return false;
        }
        char c = *(char*)phys;
        dst[i++] = c;
        user_addr++;
        if (c == '\0') return true;
    }
    dst[dst_size - 1] = '\0';
    return true;
}
}  // namespace

HguiHandler* HguiHandler::activeInstance = nullptr;

HguiHandler::HguiHandler(uint8_t InterruptNumber, InterruptManager* interruptManager)
    : InterruptHandler(InterruptNumber + 0x20, interruptManager) {
    this->activeInstance = this;

    if (Desktop::activeInstance) {
        HguiWidgets.Add(Desktop::activeInstance);
    }
}

HguiHandler::~HguiHandler() {}

uint32_t HguiHandler::HandleInterrupt(uint32_t esp) {
    CPUState* cpu = (CPUState*)esp;
    int32_t ret = -1;
    ProcessControlBlock* proc =
        Scheduler::activeInstance ? Scheduler::activeInstance->GetCurrentProcess() : nullptr;
    WidgetData data;
    const WidgetData* data_ptr = nullptr;

    if (cpu->eax != EVENT) {
        if (!proc || !CopyFromUser(proc, &data, (void*)cpu->ecx, sizeof(WidgetData))) {
            cpu->eax = (uint32_t)-1;
            return esp;
        }
        data_ptr = &data;
    }

    switch ((uint32_t)cpu->eax) {
        case WIDGET:
            ret = HandleWidget(cpu, data_ptr);
            break;
        case WINDOW:
            ret = HandleWindow(cpu, data_ptr);
            break;
        case BUTTON:
            ret = HandleButton(cpu, data_ptr);
            break;
        case LABEL:
            ret = HandleLabel(cpu, data_ptr);
            break;
        case LISTVIEW:
            ret = HandleListView(cpu, data_ptr);
            break;
        case TERMINAL_VIEW:
            ret = HandleTerminalView(cpu, data_ptr);
            break;
        case EVENT:
            ret = HandleEvent(cpu);
            break;

        default:
            break;
    }

    cpu->eax = (uint32_t)ret;
    return esp;
}

int32_t HguiHandler::HandleWidget(CPUState* cpu, const WidgetData* _data) {
    if (!cpu || !_data) return -1;

    if ((uint32_t)cpu->ebx == ADD_CHILD) {
        Widget* parentBase = this->FindWidgetByID(_data->param0);
        if (!parentBase || !parentBase->IsComposite()) return -1;
        CompositeWidget* parentWidget = static_cast<CompositeWidget*>(parentBase);

        Widget* childWidget = this->FindWidgetByID(_data->param1);
        if (!childWidget) return -1;  // Also fixes a bug where parentWidget was checked twice

        // TODO: Add checks for widgets before add to desktop
        parentWidget->AddChild(childWidget);

        // If adding a window to the Desktop, create a taskbar tab
        if (parentWidget->ID == 0) {
            Desktop* desktop = Desktop::activeInstance;
            if (desktop && desktop->GetTaskbar()) {
                // Safely check if childWidget is a Window before casting
                const char* tabTitle = "App";
                // Only Window widgets have getWindowTitle(); check by attempting
                // dynamic-like cast: verify the widget is actually a Window.
                // Since we don't have RTTI, we check if its parent is Desktop (ID==0)
                // and the widget itself responds to window semantics.
                if (childWidget->IsComposite()) {
                    // Might be a Window — try the getWindowTitle accessor
                    Window* win = static_cast<Window*>(childWidget);
                    if (win && win->getWindowTitle()) {
                        tabTitle = win->getWindowTitle();
                    }
                }
                desktop->GetTaskbar()->AddTab(childWidget->PID, tabTitle, childWidget);
            }
        }

        return 1;
    } else if ((uint32_t)cpu->ebx == DELETE) {
        Widget* target = this->FindWidgetByID(_data->param1);
        if (target) {
            // Detach from parent first
            if (target->parent) {
                target->parent->RemoveChild(target);
            }
            // Remove from global widget list
            HguiWidgets.Remove([&](Widget* c) { return c->ID == _data->param1; });
            // Destroy the widget
            delete target;
        }
        return 1;
    }

    return -1;
}

int32_t HguiHandler::HandleWindow(CPUState* cpu, const WidgetData* _data) {
    if (!cpu || !_data) return -1;
    ProcessControlBlock* proc = Scheduler::activeInstance->GetCurrentProcess();
    char titleBuf[MAX_USER_TEXT];

    if ((uint32_t)cpu->ebx == CREATE) {
        Widget* parentBase = this->FindWidgetByID(_data->param0);
        if (!parentBase || !parentBase->IsComposite()) return -1;
        CompositeWidget* parentWidget = static_cast<CompositeWidget*>(parentBase);

        uint32_t _newID = this->getNewID();
        Widget* _widget =
            new Window(parentWidget, _data->param1, _data->param2, _data->param3, _data->param4);
        if (!_widget) {
            HALT("CRITICAL: Failed to allocate Window widget!\n");
        }
        _widget->SetPID(Scheduler::activeInstance->GetCurrentProcess()->pid);
        _widget->SetID(_newID);

        HguiWidgets.Add(_widget);
        return (uint32_t)_newID;
    } else if ((uint32_t)cpu->ebx == SET_TEXT) {
        Widget* w = this->FindWidgetByID(_data->param0);
        if (!w || !w->IsComposite()) return -1;
        Window* widget = static_cast<Window*>(w);
        if (!CopyUserString(proc, _data->param5, titleBuf, sizeof(titleBuf))) {
            return -1;
        }
        widget->setWindowTitle(titleBuf);

        return 1;
    }

    return -1;
}

int32_t HguiHandler::HandleButton(CPUState* cpu, const WidgetData* _data) {
    if (!cpu || !_data) return -1;
    ProcessControlBlock* proc = Scheduler::activeInstance->GetCurrentProcess();
    char labelBuf[MAX_USER_TEXT];

    if ((uint32_t)cpu->ebx == CREATE) {
        Widget* parentBase = this->FindWidgetByID(_data->param0);
        if (!parentBase || !parentBase->IsComposite())
            return -1;  // Minor structural fix for bitwise vs Logical or originally
        CompositeWidget* parentWidget = static_cast<CompositeWidget*>(parentBase);
        if (parentWidget->ID == 0) return -1;

        if (!CopyUserString(proc, _data->param5, labelBuf, sizeof(labelBuf))) {
            return -1;
        }
        uint32_t _newID = this->getNewID();
        Widget* _widget = new Button(parentWidget, _data->param1, _data->param2, _data->param3,
                                     _data->param4, labelBuf);
        if (!_widget) {
            HALT("CRITICAL: Failed to allocate Button widget!\n");
        }
        _widget->SetPID(Scheduler::activeInstance->GetCurrentProcess()->pid);
        _widget->SetID(_newID);

        HguiWidgets.Add(_widget);
        return (int32_t)_newID;
    }

    return -1;
}

int32_t HguiHandler::HandleLabel(CPUState* cpu, const WidgetData* _data) {
    if (!cpu || !_data) return -1;
    ProcessControlBlock* proc = Scheduler::activeInstance->GetCurrentProcess();
    char textBuf[MAX_USER_TEXT];

    if ((uint32_t)cpu->ebx == CREATE) {
        Widget* parentBase = this->FindWidgetByID(_data->param0);
        if (!parentBase || !parentBase->IsComposite()) return -1;
        CompositeWidget* parentWidget = static_cast<CompositeWidget*>(parentBase);
        if (parentWidget->ID == 0) return -1;

        if (!CopyUserString(proc, _data->param5, textBuf, sizeof(textBuf))) {
            return -1;
        }
        uint32_t _newID = this->getNewID();
        Widget* _widget = new Label(parentWidget, _data->param1, _data->param2, _data->param3,
                                    _data->param4, textBuf);
        if (!_widget) {
            HALT("CRITICAL: Failed to allocate Label widget!\n");
        }
        _widget->SetPID(Scheduler::activeInstance->GetCurrentProcess()->pid);
        _widget->SetID(_newID);

        HguiWidgets.Add(_widget);
        return (int32_t)_newID;
    } else if ((uint32_t)cpu->ebx == SET_TEXT) {
        Widget* w = this->FindWidgetByID(_data->param0);
        if (!w || w->IsComposite()) return -1;
        Label* widget = static_cast<Label*>(w);
        if (!CopyUserString(proc, _data->param5, textBuf, sizeof(textBuf))) {
            return -1;
        }
        widget->setText(textBuf);

        return 1;
    } else if ((uint32_t)cpu->ebx == SET_FONT_SIZE) {
        Widget* w = this->FindWidgetByID(_data->param0);
        if (!w || w->IsComposite()) return -1;
        Label* widget = static_cast<Label*>(w);

        widget->setSize((FontSize)_data->param1);

        return 1;
    }

    return -1;
}

int32_t HguiHandler::HandleListView(CPUState* cpu, const WidgetData* _data) {
    if (!cpu || !_data) return -1;
    ProcessControlBlock* proc = Scheduler::activeInstance->GetCurrentProcess();
    char headerBuf[MAX_USER_TEXT];

    if ((uint32_t)cpu->ebx == CREATE) {
        Widget* parentBase = this->FindWidgetByID(_data->param0);
        if (!parentBase || !parentBase->IsComposite()) return -1;
        CompositeWidget* parentWidget = static_cast<CompositeWidget*>(parentBase);
        if (parentWidget->ID == 0) return -1;

        uint32_t _newID = this->getNewID();
        Widget* _widget =
            new ListView(parentWidget, _data->param1, _data->param2, _data->param3, _data->param4);
        if (!_widget) {
            HALT("CRITICAL: Failed to allocate ListView widget!\n");
        }
        _widget->SetPID(Scheduler::activeInstance->GetCurrentProcess()->pid);
        _widget->SetID(_newID);

        HguiWidgets.Add(_widget);
        return (int32_t)_newID;
    } else if ((uint32_t)cpu->ebx == SET_ITEMS) {
        // param0 = widgetID, param5 = pointer to ListViewItemData array, param1 = count
        Widget* w = this->FindWidgetByID(_data->param0);
        if (!w || w->IsComposite()) return -1;
        ListView* widget = static_cast<ListView*>(w);

        struct ListViewItemData {
            char name[64];
            uint32_t size;
            uint8_t type;
        };

        widget->Clear();
        int count = (int)_data->param1;
        if (count < 0) return -1;
        if (count > LISTVIEW_MAX_ITEMS) count = LISTVIEW_MAX_ITEMS;

        ListViewItemData items[LISTVIEW_MAX_ITEMS];
        size_t bytes = (size_t)count * sizeof(ListViewItemData);
        if (bytes > 0 && !CopyFromUser(proc, items, _data->param5, bytes)) {
            return -1;
        }

        for (int i = 0; i < count; i++) {
            items[i].name[63] = 0;
            widget->AddItem(items[i].name, items[i].size, items[i].type);
        }
        return count;
    } else if ((uint32_t)cpu->ebx == CLEAR_ITEMS) {
        Widget* w = this->FindWidgetByID(_data->param0);
        if (!w || w->IsComposite()) return -1;
        ListView* widget = static_cast<ListView*>(w);
        widget->Clear();
        return 1;
    } else if ((uint32_t)cpu->ebx == GET_SELECTED) {
        Widget* w = this->FindWidgetByID(_data->param0);
        if (!w || w->IsComposite()) return -1;
        ListView* widget = static_cast<ListView*>(w);
        return widget->GetSelectedIndex();
    } else if ((uint32_t)cpu->ebx == SET_TEXT) {
        Widget* w = this->FindWidgetByID(_data->param0);
        if (!w || w->IsComposite()) return -1;
        ListView* widget = static_cast<ListView*>(w);
        if (!CopyUserString(proc, _data->param5, headerBuf, sizeof(headerBuf))) {
            return -1;
        }
        widget->SetHeader(headerBuf);
        return 1;
    }

    return -1;
}

int32_t HguiHandler::HandleTerminalView(CPUState* cpu, const WidgetData* _data) {
    if (!cpu || !_data) return -1;
    ProcessControlBlock* proc = Scheduler::activeInstance->GetCurrentProcess();
    char textBuf[MAX_USER_TEXT];

    if ((uint32_t)cpu->ebx == CREATE) {
        Widget* parentBase = this->FindWidgetByID(_data->param0);
        if (!parentBase || !parentBase->IsComposite()) return -1;
        CompositeWidget* parentWidget = static_cast<CompositeWidget*>(parentBase);
        if (parentWidget->ID == 0) return -1;

        if (!CopyUserString(proc, _data->param5, textBuf, sizeof(textBuf))) {
            return -1;
        }
        uint32_t _newID = this->getNewID();
        Widget* _widget =
            new TerminalView(parentWidget, (int32_t)_data->param1, (int32_t)_data->param2,
                             (int32_t)_data->param3, (int32_t)_data->param4, textBuf);
        if (!_widget) {
            HALT("CRITICAL: Failed to allocate TerminalView widget!\n");
        }
        _widget->SetPID(Scheduler::activeInstance->GetCurrentProcess()->pid);
        _widget->SetID(_newID);

        HguiWidgets.Add(_widget);
        return (int32_t)_newID;
    } else if ((uint32_t)cpu->ebx == SET_TEXT) {
        Widget* w = this->FindWidgetByID(_data->param0);
        if (!w || w->IsComposite()) return -1;
        TerminalView* widget = static_cast<TerminalView*>(w);
        if (!CopyUserString(proc, _data->param5, textBuf, sizeof(textBuf))) {
            return -1;
        }
        widget->setText(textBuf);
        return 1;
    } else if ((uint32_t)cpu->ebx == SET_FONT_SIZE) {
        Widget* w = this->FindWidgetByID(_data->param0);
        if (!w || w->IsComposite()) return -1;
        TerminalView* widget = static_cast<TerminalView*>(w);
        widget->setSize((FontSize)_data->param1);
        return 1;
    } else if ((uint32_t)cpu->ebx == SET_SCROLL_META) {
        Widget* w = this->FindWidgetByID(_data->param0);
        if (!w || w->IsComposite()) return -1;
        TerminalView* widget = static_cast<TerminalView*>(w);
        widget->setScrollMeta((int)_data->param1, (int)_data->param2, (int)_data->param3);
        return 1;
    } else if ((uint32_t)cpu->ebx == GET_SCROLL_ACTION) {
        Widget* w = this->FindWidgetByID(_data->param0);
        if (!w || w->IsComposite()) return -1;
        TerminalView* widget = static_cast<TerminalView*>(w);
        return widget->consumeScrollAction();
    }

    return -1;
}

int32_t HguiHandler::HandleEvent(CPUState* cpu) {
    if (!cpu) return -1;

    if (!Scheduler::activeInstance || !Desktop::activeInstance) {
        return -1;
    }

    ProcessControlBlock* p = Scheduler::activeInstance->GetCurrentProcess();
    if (!p) {
        return -1;
    }

    EventHandler* process_eventHandler = Desktop::activeInstance->getHandler(p->pid);
    if (!process_eventHandler) {
        return -1;
    }

    if ((uint32_t)cpu->ebx == GET) {
        if ((uint32_t)process_eventHandler->eventQueue.GetSize() > 0) {
            Event* tmp = process_eventHandler->eventQueue.PopFront();
            if (!tmp) {
                return -1;
            }

            int32_t encoded = (int32_t)((tmp->widgetID << 16) | tmp->eventType);
            delete tmp;
            return encoded;
        } else {
            // No pending events; return immediately so user-space can poll other inputs.
            return -1;
        }
    }

    return -1;
}

Widget* HguiHandler::FindWidgetByID(uint32_t searchID) {
    Widget* result = nullptr;
    HguiWidgets.ForEach([&](Widget* c) {
        if (c->ID == searchID) result = c;
    });

    return result;
}

void HguiHandler::RemoveAppByPID(uint32_t PID) {
    HguiWidgets.Remove([&](Widget* c) { return c->PID == PID; });

    if (Desktop::activeInstance) {
        Desktop::activeInstance->deleteEventHandler(PID);
    }
}

uint32_t HguiHandler::getNewID() {
    return widgetIDCounter++;
}
