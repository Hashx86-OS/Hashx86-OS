/*
 * MIT License
 *
 * Copyright (c) 2025 Malaka Gunawardana
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <Hx86/Hgui/Hgui.h>
#include <Hx86/Hx86.h>
#include <Hx86/debug.h>
#include <prog.h>

HX86_DECLARE_APP(HX86_APP_GUI);

Label* l1;

// Window layout offsets.
#define TOP_PADDING 120
#define LEFT_PADDING 10

/**
 * class MemoryViewer - Reads kernel memory through the syscall interface.
 *
 * Presents an address input pad (hex digits plus control buttons), a read-size
 * selector (byte/word/dword) and labels for the address and its value.
 */
class MemoryViewer {
private:
    Window* mainWindow;
    Label* addressScreen;
    Label* valueScreen;
    Button *btn_0, *btn_1, *btn_2, *btn_3, *btn_4, *btn_5, *btn_6, *btn_7, *btn_8, *btn_9;
    Button *btn_a, *btn_b, *btn_c, *btn_d, *btn_e, *btn_f;
    Button *btn_clear, *btn_backspace;
    Button *btn_byte, *btn_word, *btn_dword;

    char addressInput[32];
    int inputIndex;
    int readSize;  // 1 = byte, 2 = word, 4 = dword.

public:
    MemoryViewer();
    ~MemoryViewer();
    void onPressHex(char hex);
    void onPressClear();
    void onPressBackspace();
    void onPressRead();
    void onPressSize(int size);
    void clearInput();
};

/**
 * atoi() - Parse a signed decimal string.
 * @str: Null-terminated string, optionally starting with '-'.
 *
 * Return: The parsed integer value.
 */
int atoi(const char* str) {
    int res = 0;
    int sign = 1;
    if (*str == '-') {
        sign = -1;
        str++;
    }
    while (*str >= '0' && *str <= '9') {
        res = res * 10 + (*str - '0');
        str++;
    }
    return res * sign;
}

/**
 * hextoi() - Parse a hexadecimal string.
 * @str: Null-terminated string of hex digits.
 *
 * Return: The parsed unsigned value.
 */
uint32_t hextoi(const char* str) {
    uint32_t result = 0;
    while (*str) {
        char c = *str;
        if (c >= '0' && c <= '9') {
            result = result * 16 + (c - '0');
        } else if (c >= 'A' && c <= 'F') {
            result = result * 16 + (c - 'A' + 10);
        } else if (c >= 'a' && c <= 'f') {
            result = result * 16 + (c - 'a' + 10);
        }
        str++;
    }
    return result;
}

/**
 * itohex() - Format a value as zero-padded uppercase hex.
 * @value: Value to format.
 * @str: Destination buffer, at least @width + 1 bytes.
 * @width: Number of hex digits to produce.
 */
void itohex(uint32_t value, char* str, int width = 8) {
    const char hexchars[] = "0123456789ABCDEF";
    str[width] = '\0';

    for (int i = width - 1; i >= 0; i--) {
        str[i] = hexchars[value & 0xF];
        value >>= 4;
    }
}

/**
 * itoa() - Format a signed integer as a decimal string.
 * @value: Value to format.
 * @str: Destination buffer.
 */
void itoa(int value, char* str) {
    char* p = str;
    bool isNegative = false;

    if (value == 0) {
        *p++ = '0';
        *p = '\0';
        return;
    }

    if (value < 0) {
        isNegative = true;
        value = -value;
    }

    // Convert the digits in reverse order.
    char* start = p;
    while (value > 0) {
        *p++ = '0' + (value % 10);
        value /= 10;
    }

    if (isNegative) *p++ = '-';
    *p = '\0';

    // Reverse the digit string.
    char* end = p - 1;
    while (start < end) {
        char tmp = *start;
        *start++ = *end;
        *end-- = tmp;
    }
}

/**
 * MemoryViewer() - Build the viewer window and wire up its buttons.
 */
MemoryViewer::MemoryViewer() {
    mainWindow = new Window(desktop, 480, 340, 255, 265);
    mainWindow->setWindowTitle("MeM Viewer 1.0.0");

    // Address input screen.
    addressScreen = new Label(mainWindow, 10, 20, 230, 35, "0x00000000");
    addressScreen->setSize(LARGE);

    // Value display screen.
    valueScreen = new Label(mainWindow, 10, 45, 230, 35, "Value: --");
    valueScreen->setSize(LARGE);

    inputIndex = 0;
    addressInput[0] = '\0';
    readSize = 1;  // Default to byte.

    // Size selection buttons.
    btn_byte = new Button(mainWindow, LEFT_PADDING, 85, 60, 25, "BYTE");
    btn_word = new Button(mainWindow, LEFT_PADDING + 70, 85, 60, 25, "WORD");
    btn_dword = new Button(mainWindow, LEFT_PADDING + 140, 85, 60, 25, "DWORD");

    // Hex digit buttons (0-9, A-F).
    btn_0 = new Button(mainWindow, LEFT_PADDING + 0 * 40, (TOP_PADDING + 3 * 35), 35, 30, "0");
    btn_1 = new Button(mainWindow, LEFT_PADDING + 1 * 40, (TOP_PADDING + 3 * 35), 35, 30, "1");
    btn_2 = new Button(mainWindow, LEFT_PADDING + 2 * 40, (TOP_PADDING + 3 * 35), 35, 30, "2");
    btn_3 = new Button(mainWindow, LEFT_PADDING + 3 * 40, (TOP_PADDING + 3 * 35), 35, 30, "3");
    btn_4 = new Button(mainWindow, LEFT_PADDING + 4 * 40, (TOP_PADDING + 3 * 35), 35, 30, "4");
    btn_5 = new Button(mainWindow, LEFT_PADDING + 5 * 40, (TOP_PADDING + 3 * 35), 35, 30, "5");

    btn_6 = new Button(mainWindow, LEFT_PADDING + 0 * 40, (TOP_PADDING + 2 * 35), 35, 30, "6");
    btn_7 = new Button(mainWindow, LEFT_PADDING + 1 * 40, (TOP_PADDING + 2 * 35), 35, 30, "7");
    btn_8 = new Button(mainWindow, LEFT_PADDING + 2 * 40, (TOP_PADDING + 2 * 35), 35, 30, "8");
    btn_9 = new Button(mainWindow, LEFT_PADDING + 3 * 40, (TOP_PADDING + 2 * 35), 35, 30, "9");
    btn_a = new Button(mainWindow, LEFT_PADDING + 4 * 40, (TOP_PADDING + 2 * 35), 35, 30, "A");
    btn_b = new Button(mainWindow, LEFT_PADDING + 5 * 40, (TOP_PADDING + 2 * 35), 35, 30, "B");

    btn_c = new Button(mainWindow, LEFT_PADDING + 0 * 40, (TOP_PADDING + 1 * 35), 35, 30, "C");
    btn_d = new Button(mainWindow, LEFT_PADDING + 1 * 40, (TOP_PADDING + 1 * 35), 35, 30, "D");
    btn_e = new Button(mainWindow, LEFT_PADDING + 2 * 40, (TOP_PADDING + 1 * 35), 35, 30, "E");
    btn_f = new Button(mainWindow, LEFT_PADDING + 3 * 40, (TOP_PADDING + 1 * 35), 35, 30, "F");
    btn_backspace =
        new Button(mainWindow, LEFT_PADDING + 4 * 40, (TOP_PADDING + 1 * 35), 75, 30, "BACK");

    // Control buttons.
    btn_clear =
        new Button(mainWindow, LEFT_PADDING + 0 * 40, (TOP_PADDING + 0 * 45), 65, 30, "CLEAR");

    // Add all children to the window.
    mainWindow->AddChild(addressScreen);
    mainWindow->AddChild(valueScreen);
    mainWindow->AddChild(btn_byte);
    mainWindow->AddChild(btn_word);
    mainWindow->AddChild(btn_dword);
    mainWindow->AddChild(btn_0);
    mainWindow->AddChild(btn_1);
    mainWindow->AddChild(btn_2);
    mainWindow->AddChild(btn_3);
    mainWindow->AddChild(btn_4);
    mainWindow->AddChild(btn_5);
    mainWindow->AddChild(btn_6);
    mainWindow->AddChild(btn_7);
    mainWindow->AddChild(btn_8);
    mainWindow->AddChild(btn_9);
    mainWindow->AddChild(btn_a);
    mainWindow->AddChild(btn_b);
    mainWindow->AddChild(btn_c);
    mainWindow->AddChild(btn_d);
    mainWindow->AddChild(btn_e);
    mainWindow->AddChild(btn_f);
    mainWindow->AddChild(btn_clear);
    mainWindow->AddChild(btn_backspace);

    // Set up the click handlers for the hex digits.
    btn_0->OnClick(this,
                   [](void* instance) { static_cast<MemoryViewer*>(instance)->onPressHex('0'); });
    btn_1->OnClick(this,
                   [](void* instance) { static_cast<MemoryViewer*>(instance)->onPressHex('1'); });
    btn_2->OnClick(this,
                   [](void* instance) { static_cast<MemoryViewer*>(instance)->onPressHex('2'); });
    btn_3->OnClick(this,
                   [](void* instance) { static_cast<MemoryViewer*>(instance)->onPressHex('3'); });
    btn_4->OnClick(this,
                   [](void* instance) { static_cast<MemoryViewer*>(instance)->onPressHex('4'); });
    btn_5->OnClick(this,
                   [](void* instance) { static_cast<MemoryViewer*>(instance)->onPressHex('5'); });
    btn_6->OnClick(this,
                   [](void* instance) { static_cast<MemoryViewer*>(instance)->onPressHex('6'); });
    btn_7->OnClick(this,
                   [](void* instance) { static_cast<MemoryViewer*>(instance)->onPressHex('7'); });
    btn_8->OnClick(this,
                   [](void* instance) { static_cast<MemoryViewer*>(instance)->onPressHex('8'); });
    btn_9->OnClick(this,
                   [](void* instance) { static_cast<MemoryViewer*>(instance)->onPressHex('9'); });
    btn_a->OnClick(this,
                   [](void* instance) { static_cast<MemoryViewer*>(instance)->onPressHex('A'); });
    btn_b->OnClick(this,
                   [](void* instance) { static_cast<MemoryViewer*>(instance)->onPressHex('B'); });
    btn_c->OnClick(this,
                   [](void* instance) { static_cast<MemoryViewer*>(instance)->onPressHex('C'); });
    btn_d->OnClick(this,
                   [](void* instance) { static_cast<MemoryViewer*>(instance)->onPressHex('D'); });
    btn_e->OnClick(this,
                   [](void* instance) { static_cast<MemoryViewer*>(instance)->onPressHex('E'); });
    btn_f->OnClick(this,
                   [](void* instance) { static_cast<MemoryViewer*>(instance)->onPressHex('F'); });

    // Set up the control button handlers.
    btn_clear->OnClick(
        this, [](void* instance) { static_cast<MemoryViewer*>(instance)->onPressClear(); });
    btn_backspace->OnClick(
        this, [](void* instance) { static_cast<MemoryViewer*>(instance)->onPressBackspace(); });

    // Set up the size button handlers.
    btn_byte->OnClick(this,
                      [](void* instance) { static_cast<MemoryViewer*>(instance)->onPressSize(1); });
    btn_word->OnClick(this,
                      [](void* instance) { static_cast<MemoryViewer*>(instance)->onPressSize(2); });
    btn_dword->OnClick(
        this, [](void* instance) { static_cast<MemoryViewer*>(instance)->onPressSize(4); });

    mainWindow->show();
}

MemoryViewer::~MemoryViewer() {
    delete mainWindow;
}

/**
 * onPressHex() - Append a hex digit to the address input.
 * @hex: Digit to append, '0'-'9' or 'A'-'F'.
 *
 * Truncates at eight digits (a 32-bit address) and re-reads the memory once
 * at least one digit has been entered.
 */
void MemoryViewer::onPressHex(char hex) {
    if (inputIndex < 8) {  // Max 8 hex digits for a 32-bit address.
        addressInput[inputIndex++] = hex;
        addressInput[inputIndex] = '\0';

        // Build the display string with proper formatting.
        char displayAddr[32] = "0x";

        // Pad with leading zeros.
        for (int i = 0; i < 8 - inputIndex; i++) {
            displayAddr[2 + i] = '0';
        }

        // Append the entered digits.
        for (int i = 0; i < inputIndex; i++) {
            displayAddr[2 + 8 - inputIndex + i] = addressInput[i];
        }
        displayAddr[10] = '\0';

        addressScreen->setText(displayAddr);

        // Auto-read the memory once at least one digit is present.
        if (inputIndex > 0) {
            onPressRead();
        }
    }
}

void MemoryViewer::onPressClear() {
    clearInput();
    valueScreen->setText("Value: --");
    printf("Memory viewer cleared\n");
}

void MemoryViewer::onPressBackspace() {
    if (inputIndex > 0) {
        inputIndex--;
        addressInput[inputIndex] = '\0';

        // Build the display string.
        char displayAddr[32] = "0x";

        // Pad with leading zeros.
        for (int i = 0; i < 8 - inputIndex; i++) {
            displayAddr[2 + i] = '0';
        }

        // Append the remaining digits.
        for (int i = 0; i < inputIndex; i++) {
            displayAddr[2 + 8 - inputIndex + i] = addressInput[i];
        }
        displayAddr[10] = '\0';

        addressScreen->setText(displayAddr);

        // Update the memory display while digits remain.
        if (inputIndex > 0) {
            onPressRead();
        } else {
            valueScreen->setText("Value: --");
        }
    }
}

/**
 * onPressRead() - Peek the memory at the entered address through a syscall.
 *
 * User mode cannot access arbitrary memory, so the value is read by the kernel
 * with syscall_peek_memory() and formatted for the current read size.
 */
void MemoryViewer::onPressRead() {
    if (inputIndex == 0) {
        valueScreen->setText("Value: --");
        return;
    }

    uint32_t address = hextoi(addressInput);
    char valueStr[64];

    // Read memory via the kernel syscall (user mode cannot access arbitrary addresses).
    uint32_t value = syscall_peek_memory(address, readSize);

    switch (readSize) {
        case 1:  // Byte.
            valueStr[0] = 'B';
            valueStr[1] = 'y';
            valueStr[2] = 't';
            valueStr[3] = 'e';
            valueStr[4] = ':';
            valueStr[5] = ' ';
            valueStr[6] = '0';
            valueStr[7] = 'x';
            itohex(value, valueStr + 8, 2);
            valueStr[10] = '\0';
            break;

        case 2:  // Word.
            valueStr[0] = 'W';
            valueStr[1] = 'o';
            valueStr[2] = 'r';
            valueStr[3] = 'd';
            valueStr[4] = ':';
            valueStr[5] = ' ';
            valueStr[6] = '0';
            valueStr[7] = 'x';
            itohex(value, valueStr + 8, 4);
            valueStr[12] = '\0';
            break;

        case 4:  // Dword.
            valueStr[0] = 'D';
            valueStr[1] = 'w';
            valueStr[2] = 'o';
            valueStr[3] = 'r';
            valueStr[4] = 'd';
            valueStr[5] = ':';
            valueStr[6] = ' ';
            valueStr[7] = '0';
            valueStr[8] = 'x';
            itohex(value, valueStr + 9, 8);
            valueStr[17] = '\0';
            break;
    }

    valueScreen->setText(valueStr);
    printf("Read from 0x%x: 0x%x (%d bytes)\n", address, value, readSize);
}

/**
 * onPressSize() - Change the read width (1, 2 or 4 bytes).
 * @size: New read size in bytes.
 */
void MemoryViewer::onPressSize(int size) {
    readSize = size;
    printf("Read size set to %d bytes\n", size);

    // Re-read the memory with the new size while an address is set.
    if (inputIndex > 0) {
        onPressRead();
    }
}

void MemoryViewer::clearInput() {
    inputIndex = 0;
    addressInput[0] = '\0';
    addressScreen->setText("0x00000000");
}

/**
 * inb() - Read a byte from an I/O port.
 * @portNumber: Port to read from.
 *
 * Helper used by the serial debug output.
 *
 * Return: The byte read from the port.
 */
uint8_t inb(uint16_t portNumber) {
    uint8_t result;
    asm volatile("inb %1, %0" : "=a"(result) : "Nd"(portNumber));
    return result;
}

/**
 * outb() - Write a byte to an I/O port.
 * @portNumber: Port to write to.
 * @value: Byte to write.
 */
void outb(uint16_t portNumber, uint8_t value) {
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(portNumber));
}

/**
 * writeSerial() - Send a single character over COM1.
 * @c: Character to transmit.
 */
void writeSerial(char c) {
    while ((inb(0x3F8 + 5) & 0x20) == 0);  // Wait for the transmit buffer to empty.
    outb(0x3F8, c);
}

/**
 * _start() - Application entry point for the memory viewer.
 * @arg: Program arguments passed by the loader.
 *
 * Initializes the system and graphics, then launches the MemoryViewer window
 * and returns, leaving the GUI event loop to run.
 */
extern "C" void _start(void* arg) {
    init_sys(arg);
    init_graphics();

    printf("[Memory Viewer]\n");

    MemoryViewer* mv = new MemoryViewer();
}
