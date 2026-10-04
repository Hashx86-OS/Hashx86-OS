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

#define KDBG_COMPONENT "GUI:MSGBOX"
#include <gui/desktop.h>
#include <gui/messagebox.h>

// Geometry inside the dialog, relative to its top-left corner.
#define MSGBOX_TEXT_X 16
#define MSGBOX_TEXT_Y 46
#define MSGBOX_TEXT_W (MSGBOXWIDTH - MSGBOX_TEXT_X - 14)
#define MSGBOX_BTN_W 84
#define MSGBOX_BTN_H 24
#define MSGBOX_BTN_Y (MSGBOXHEIGHT - MSGBOX_BTN_H - 14)

/**
 * confirmTrampoline() - ACButton trampoline for the confirm button.
 * @instance: The MessageBox that owns the button.
 *
 * Context: Runs on mouse release inside the GUI task.
 */
static void confirmTrampoline(void* instance) {
    static_cast<MessageBox*>(instance)->OnConfirmClicked();
}

/**
 * cancelTrampoline() - ACButton trampoline for the cancel button.
 * @instance: The MessageBox that owns the button.
 *
 * Context: Runs on mouse release inside the GUI task.
 */
static void cancelTrampoline(void* instance) {
    static_cast<MessageBox*>(instance)->OnCancelClicked();
}

MessageBox::MessageBox(CompositeWidget* parent, int32_t width, int32_t height)
    : Window(parent, 0, 0, width, height),
      messageLabel(nullptr),
      confirmButton(nullptr),
      cancelButton(nullptr),
      onConfirmAction(nullptr),
      confirmInstance(nullptr) {
    // Placeholder position; ShowConfirm() re-centres against the live desktop
    // every time the dialog is opened.
    x = (GUI_SCREEN_WIDTH - w) / 2;
    y = (GUI_SCREEN_HEIGHT - h) / 2;

    SetPID(0);  // Kernel/system owned, so app cleanup never tears it down.
    setWindowTitle("Confirm");

    messageLabel = new Label(this, MSGBOX_TEXT_X, MSGBOX_TEXT_Y, MSGBOX_TEXT_W, 24, "");
    confirmButton = new ACButton(this, w - MSGBOX_BTN_W - 14, MSGBOX_BTN_Y, "");
    cancelButton = new ACButton(this, w - (MSGBOX_BTN_W * 2) - 26, MSGBOX_BTN_Y, "Cancel");

    if (!messageLabel || !confirmButton || !cancelButton) {
        HALT("CRITICAL: Failed to allocate message box widgets!\n");
    }

    messageLabel->setSize(MEDIUM);

    confirmButton->SetWidth(MSGBOX_BTN_W);
    confirmButton->SetHeight(MSGBOX_BTN_H);
    cancelButton->SetWidth(MSGBOX_BTN_W);
    cancelButton->SetHeight(MSGBOX_BTN_H);

    confirmButton->OnClick(this, confirmTrampoline);
    cancelButton->OnClick(this, cancelTrampoline);

    AddChild(messageLabel);
    AddChild(confirmButton);
    AddChild(cancelButton);

    // A confirmation is answered with its buttons, not by closing the window.
    closeButton->isVisible = false;

    // Created hidden; the first ShowConfirm() makes it visible.
    setVisible(false);
}

/**
 * MessageBox::~MessageBox() - Destroy the dialog.
 *
 * Defined out of line so this translation unit provides the vtable anchor.
 * Children added via AddChild() are owned and freed by the composite tree.
 */
MessageBox::~MessageBox() {}

void MessageBox::OnConfirmClicked() {
    MsgBoxAction action = onConfirmAction;
    void* instance = confirmInstance;

    // Clear first: the action typically never returns, and a second click
    // must not be able to run it again.
    onConfirmAction = nullptr;
    confirmInstance = nullptr;

    HideDialog();

    // This runs on the IRQ 12 path, so the action cannot be called here: it must
    // not execute in interrupt context, and the machine would otherwise halt or
    // reset with the dialog still on screen, since it never returns and the
    // frame that would erase it is only drawn on the next desktop-task pass.
    // Queue it behind that repaint instead. HideDialog() already marked the
    // desktop dirty, so a frame is guaranteed to be drawn.
    Desktop* desktop = Desktop::activeInstance;
    if (desktop) {
        desktop->PostAfterPresent(action, instance);
    } else {
        // No desktop to sequence against (headless); preserve the old behaviour.
        if (action) action(instance);
    }
}

void MessageBox::OnCancelClicked() {
    onConfirmAction = nullptr;
    confirmInstance = nullptr;
    HideDialog();
}

/**
 * MessageBox::ShowConfirm() - Present a confirmation prompt.
 * @title: Title-bar text.
 * @message: Single-line question shown in the body.
 * @confirmLabel: Label for the button that runs the action.
 * @action: Callback run when the user confirms.
 * @instance: Opaque pointer passed to @action.
 */
void MessageBox::ShowConfirm(const char* title, const char* message, const char* confirmLabel,
                             MsgBoxAction action, void* instance) {
    setWindowTitle(title ? title : "Confirm");
    messageLabel->setText(message ? message : "");
    confirmButton->SetLabel(confirmLabel ? confirmLabel : "OK");
    cancelButton->isVisible = true;

    onConfirmAction = action;
    confirmInstance = instance;

    Desktop* desktop = Desktop::activeInstance;
    if (desktop) {
        x = (desktop->w - w) / 2;
        y = (desktop->h - h) / 2;
    }

    showDialog();
    // Take focus so the prompt is raised to the top of the Z-order. This dialog
    // is created once and reused, so the first prompt lands on top only by
    // virtue of being added last; any window focused since then was pushed in
    // front of it and would otherwise be drawn over the prompt.
    if (desktop) {
        desktop->GetFocus(this);
        holdsFocus = true;
        // Route all further mouse input here so the windows behind cannot be
        // clicked through the prompt.
        desktop->SetModalWidget(this);
    }
    MarkDirty();
}

/**
 * MessageBox::showDialog() - Make the dialog visible, close button suppressed.
 *
 * Window::setVisible() mirrors the window state onto its close button, so the
 * button is re-hidden after every show: a confirmation is answered with its
 * buttons, never by closing the window out from under the prompt.
 */
void MessageBox::showDialog() {
    setVisible(true);
    closeButton->isVisible = false;
}

/**
 * MessageBox::HideDialog() - Close the dialog and release the modal input lock.
 */
void MessageBox::HideDialog() {
    // Only drop the lock if this dialog currently owns it; a newer prompt may
    // already have taken over.
    Desktop* desktop = Desktop::activeInstance;
    if (desktop && desktop->GetModalWidget() == this) {
        desktop->SetModalWidget(nullptr);
    }

    // Hand back the focus taken by ShowConfirm(). A hidden widget left as the
    // desktop's focused child would swallow keystrokes, and CompositeWidget's
    // focus path treats "already focused" as a no-op, so the next ShowConfirm()
    // would skip the raise that puts this dialog back on top.
    if (desktop && holdsFocus) {
        desktop->GetFocus(nullptr);
        holdsFocus = false;
    }

    onConfirmAction = nullptr;
    confirmInstance = nullptr;

    setVisible(false);
    MarkDirty();
}
