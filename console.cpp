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

#include <console.h>

int cursorRow = 0;
int cursorCol = 0;

Port8Bit port_1(0x3D4);
Port8Bit port_2(0x3D5);

/**
 * combineColors() - Merge foreground and background colors into one byte.
 * @foreground: Foreground color code in the low nibble.
 * @background: Background color code in the high nibble.
 *
 * Return: The combined color byte.
 */
TextColor combineColors(TextColor foreground, TextColor background) {
    return (TextColor)((background << 4) | foreground);
}

/**
 * MSGPrintf() - Print a tagged message with a custom tag color.
 * @cTag: Color used for the printed tag.
 * @tag: Tag identifying the module or context.
 * @format: printf-style format string.
 * @...: Arguments referenced by the format string.
 */
void MSGPrintf(TextColor cTag, const char* tag, const char* format, ...) {
    printf(cTag, "[%s]", tag);  // Print the tag.
    printf(LIGHT_GRAY, ":");
    if (!format || !*format) {
        return;  // Do nothing if the format is null or empty.
    }

    // Check if the format string contains placeholders.
    bool hasPlaceholders = false;
    for (const char* p = format; *p != '\0'; p++) {
        if (*p == '%') {
            hasPlaceholders = true;
            break;
        }
    }

    // If there are no placeholders, print the format string as is.
    if (!hasPlaceholders) {
        printf(LIGHT_GRAY, format);
        return;
    }

    // Process the format string with arguments.
    va_list args;
    va_start(args, format);

    for (const char* p = format; *p != '\0'; p++) {
        if (*p == '%') {
            p++;
            // Check for end-of-string after the increment.
            if (*p == '\0') break;
            switch (*p) {
                case 'c': {  // Character.
                    int ch = va_arg(args, int);
                    printf(LIGHT_GRAY, "%c", (char)ch);
                    break;
                }
                case 's': {  // String.
                    const char* str = va_arg(args, const char*);
                    if (!str) str = "(null)";
                    printf(LIGHT_GRAY, "%s", str);
                    break;
                }
                case 'd': {  // Decimal integer.
                    int num = va_arg(args, int);
                    printf(LIGHT_GRAY, "%d", num);
                    break;
                }
                case 'x': {  // Hexadecimal.
                    int num = va_arg(args, int);
                    printf(LIGHT_GRAY, "%x", num);
                    break;
                }
                case '%': {  // Literal percent.
                    printf(LIGHT_GRAY, "%c", '%');
                    break;
                }
                default:
                    printf(LIGHT_GRAY, "%c", '%');
                    printf(LIGHT_GRAY, "%c", *p);
                    break;
            }
        } else {
            printf(LIGHT_GRAY, "%c", *p);
        }
    }

    va_end(args);
}

/**
 * scrollScreen() - Scroll all text rows up by one and clear the last row.
 */
void scrollScreen() {
    unsigned short* VideoMemory = (unsigned short*)VIDEO_MEMORY_ADDRESS;

    // Scroll all rows up by one.
    for (int row = 1; row < SCREEN_HEIGHT; row++) {
        for (int col = 0; col < SCREEN_WIDTH; col++) {
            VideoMemory[(row - 1) * SCREEN_WIDTH + col] = VideoMemory[row * SCREEN_WIDTH + col];
        }
    }

    // Clear the last row.
    unsigned short blank = 0x20 | (WHITE << 8);  // Space with white text on a black background.
    for (int col = 0; col < SCREEN_WIDTH; col++) {
        VideoMemory[(SCREEN_HEIGHT - 1) * SCREEN_WIDTH + col] = blank;
    }

    // Clamp the cursor row back into range.
    if (cursorRow > SCREEN_HEIGHT - 1) {
        cursorRow = SCREEN_HEIGHT - 1;
    }
}

/**
 * updateCursor() - Program the VGA hardware cursor position.
 * @row: New cursor row.
 * @col: New cursor column.
 */
void updateCursor(int row, int col) {
    unsigned short position = row * SCREEN_WIDTH + col;

    // Set the cursor start and enable blinking.
    port_1.Write(0x0A);  // Cursor Start Register.
    port_2.Write(0x06);  // Start at scanline 6 (enables blinking).

    // Set the cursor end.
    port_1.Write(0x0B);  // Cursor End Register.
    port_2.Write(0x0F);  // End at scanline 15.

    // Update the cursor position.
    port_1.Write(0x0E);  // High byte of cursor position.
    port_2.Write((position >> 8) & 0xFF);

    port_1.Write(0x0F);  // Low byte of cursor position.
    port_2.Write(position & 0xFF);
}

/**
 * clearScreen() - Clear the screen and reset the cursor to the origin.
 */
void clearScreen() {
    unsigned short* VideoMemory = (unsigned short*)VIDEO_MEMORY_ADDRESS;
    unsigned short blank = 0x20 | (WHITE << 8);  // Space with white text on a black background.

    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
        VideoMemory[i] = blank;
    }

    cursorRow = 0;
    cursorCol = 0;
    updateCursor(0, 0);
}

/**
 * printf() - Print formatted text on the screen in the given color.
 * @color: Text color for the output.
 * @format: printf-style format string.
 * @...: Arguments referenced by the format string.
 *
 * Supports %d, %u, %x, %c, %s and %% conversions. The cursor advances as
 * text is written and the screen scrolls when the buffer is full.
 */
void printf(TextColor color, const char* format, ...) {
    unsigned short* VideoMemory = (unsigned short*)VIDEO_MEMORY_ADDRESS;
    va_list args;
    va_start(args, format);

    for (int i = 0; format[i] != '\0'; i++) {
        if (format[i] == '%') {
            i++;
            if (format[i] == '\0') {
                int position = cursorRow * SCREEN_WIDTH + cursorCol;
                VideoMemory[position] = (color << 8) | '%';
                break;
            }
            switch (format[i]) {
                case 'd': {  // Signed integer.
                    int num = va_arg(args, int);
                    char buffer[12];       // Enough for -2147483648 + '\0'.
                    int index = 11;        // Start filling from the end.
                    buffer[index] = '\0';  // Null-terminate.

                    if (num == 0) {
                        buffer[--index] = '0';
                    } else if (num == -2147483648) {  // Special case for INT_MIN.
                        const char* minStr = "-2147483648";
                        for (int j = 0; minStr[j] != '\0'; j++) {
                            int position = cursorRow * SCREEN_WIDTH + cursorCol;
                            VideoMemory[position] = (color << 8) | minStr[j];
                            cursorCol++;
                            if (cursorCol >= SCREEN_WIDTH) {
                                cursorCol = 0;
                                cursorRow++;
                            }
                            if (cursorRow >= SCREEN_HEIGHT) {
                                scrollScreen();
                                cursorRow = SCREEN_HEIGHT - 1;
                            }
                        }
                        break;  // Exit the case here.
                    } else {
                        bool isNegative = (num < 0);
                        if (isNegative) num = -num;

                        while (num > 0) {
                            buffer[--index] = (num % 10) + '0';
                            num /= 10;
                        }

                        if (isNegative) buffer[--index] = '-';
                    }

                    for (int j = index; buffer[j] != '\0'; j++) {
                        int position = cursorRow * SCREEN_WIDTH + cursorCol;
                        VideoMemory[position] = (color << 8) | buffer[j];
                        cursorCol++;
                        if (cursorCol >= SCREEN_WIDTH) {
                            cursorCol = 0;
                            cursorRow++;
                        }
                        if (cursorRow >= SCREEN_HEIGHT) {
                            scrollScreen();
                            cursorRow = SCREEN_HEIGHT - 1;
                        }
                    }
                    break;
                }

                case 'u': {  // Unsigned integer.
                    uint32_t num = va_arg(args, uint32_t);
                    char buffer[11];       // Enough for 0 to 4294967295.
                    int index = 10;        // Start filling from the end.
                    buffer[index] = '\0';  // Null-terminate.

                    if (num == 0) {
                        buffer[--index] = '0';
                    } else {
                        while (num > 0) {
                            buffer[--index] = (num % 10) + '0';
                            num /= 10;
                        }
                    }

                    for (int j = index; buffer[j] != '\0'; j++) {
                        int position = cursorRow * SCREEN_WIDTH + cursorCol;
                        VideoMemory[position] = (color << 8) | buffer[j];
                        cursorCol++;
                        if (cursorCol >= SCREEN_WIDTH) {
                            cursorCol = 0;
                            cursorRow++;
                        }
                        if (cursorRow >= SCREEN_HEIGHT) {
                            scrollScreen();
                            cursorRow = SCREEN_HEIGHT - 1;
                        }
                    }
                    break;
                }

                case 'x': {  // Hexadecimal.
                    uint32_t num = va_arg(args, uint32_t);
                    char buffer[9];        // Enough for 8 hex digits + '\0'.
                    int index = 8;         // Start filling from the end.
                    buffer[index] = '\0';  // Null-terminate.
                    const char* hexDigits = "0123456789ABCDEF";

                    if (num == 0) {
                        buffer[--index] = '0';
                    } else {
                        while (num > 0) {
                            buffer[--index] = hexDigits[num % 16];
                            num /= 16;
                        }
                    }

                    for (int j = index; buffer[j] != '\0'; j++) {
                        int position = cursorRow * SCREEN_WIDTH + cursorCol;
                        VideoMemory[position] = (color << 8) | buffer[j];
                        cursorCol++;
                        if (cursorCol >= SCREEN_WIDTH) {
                            cursorCol = 0;
                            cursorRow++;
                        }
                        if (cursorRow >= SCREEN_HEIGHT) {
                            scrollScreen();
                            cursorRow = SCREEN_HEIGHT - 1;
                        }
                    }
                    break;
                }

                case 'c': {  // Character.
                    int ch = va_arg(args, int);
                    int position = cursorRow * SCREEN_WIDTH + cursorCol;
                    VideoMemory[position] = (color << 8) | (char)ch;
                    cursorCol++;
                    if (cursorCol >= SCREEN_WIDTH) {
                        cursorCol = 0;
                        cursorRow++;
                    }
                    if (cursorRow >= SCREEN_HEIGHT) {
                        scrollScreen();
                        cursorRow = SCREEN_HEIGHT - 1;
                    }
                    break;
                }

                case 's': {  // String.
                    const char* str = va_arg(args, const char*);
                    if (!str) str = "(null)";
                    for (int j = 0; str[j] != '\0'; j++) {
                        int position = cursorRow * SCREEN_WIDTH + cursorCol;
                        VideoMemory[position] = (color << 8) | str[j];
                        cursorCol++;

                        if (cursorCol >= SCREEN_WIDTH) {
                            cursorCol = 0;
                            cursorRow++;
                        }

                        if (cursorRow >= SCREEN_HEIGHT) {
                            scrollScreen();
                            cursorRow = SCREEN_HEIGHT - 1;
                        }
                    }
                    break;
                }

                case '%': {
                    int position = cursorRow * SCREEN_WIDTH + cursorCol;
                    VideoMemory[position] = (color << 8) | '%';
                    cursorCol++;
                    if (cursorCol >= SCREEN_WIDTH) {
                        cursorCol = 0;
                        cursorRow++;
                    }
                    if (cursorRow >= SCREEN_HEIGHT) {
                        scrollScreen();
                        cursorRow = SCREEN_HEIGHT - 1;
                    }
                    break;
                }

                default:
                    break;
            }
        } else if (format[i] == '\n') {
            cursorRow++;
            cursorCol = 0;  // Reset the column to the beginning.

            // Scroll if the new cursor row exceeds the screen height.
            if (cursorRow >= SCREEN_HEIGHT) {
                scrollScreen();
                cursorRow = SCREEN_HEIGHT - 1;  // Move the cursor to the last row.
                cursorCol = 0;                  // Start at the beginning of the row.
            }

        } else {
            int position = cursorRow * SCREEN_WIDTH + cursorCol;
            VideoMemory[position] = (color << 8) | format[i];
            cursorCol++;

            if (cursorCol >= SCREEN_WIDTH) {
                cursorCol = 0;
                cursorRow++;
            }

            if (cursorRow >= SCREEN_HEIGHT) {
                scrollScreen();
                cursorRow = SCREEN_HEIGHT - 1;
            }
        }
    }
    va_end(args);
    updateCursor(cursorRow, cursorCol);
}
