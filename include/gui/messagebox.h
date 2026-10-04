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

#ifndef MESSAGEBOX_H
#define MESSAGEBOX_H

#include <gui/config/config.h>
#include <gui/elements/window_action_button.h>
#include <gui/label.h>
#include <gui/window.h>

/**
 * class MessageBox - Modal dialog that confirms an action before running it.
 *
 * Used to gate irreversible actions (restart, shutdown) behind an explicit
 * choice. While visible it registers itself as the desktop's modal widget, so
 * mouse input is routed only to it and the windows behind cannot be clicked
 * through it.
 *
 * The dialog keeps the close button hidden: a confirmation is dismissed with
 * Cancel, never by closing the window out from under the prompt.
 */
class MessageBox : public Window {
public:
    /**
     * MsgBoxAction - Signature of the callback run when the user confirms.
     * @instance: Opaque pointer handed back to the callback unchanged.
     */
    typedef void (*MsgBoxAction)(void* instance);

private:
    Label* messageLabel;
    ACButton* confirmButton;
    ACButton* cancelButton;
    MsgBoxAction onConfirmAction;
    void* confirmInstance;

public:
    /**
     * OnConfirmClicked() - Run the pending action, then close the dialog.
     *
     * Context: Mouse release on the confirm button, inside the GUI task.
     */
    void OnConfirmClicked();

    /**
     * OnCancelClicked() - Close the dialog without running the action.
     *
     * Context: Mouse release on the cancel button, inside the GUI task.
     */
    void OnCancelClicked();

    /**
     * MessageBox() - Construct a hidden confirmation dialog.
     * @parent: Parent composite widget, normally the Desktop.
     * @width: Dialog width in pixels.
     * @height: Dialog height in pixels.
     */
    MessageBox(CompositeWidget* parent, int32_t width = MSGBOXWIDTH, int32_t height = MSGBOXHEIGHT);

    /**
     * ~MessageBox() - Destroy the dialog and its child widgets.
     */
    ~MessageBox();

    /**
     * ShowConfirm() - Present a confirmation prompt.
     * @title: Title-bar text.
     * @message: Single-line question shown in the body.
     * @confirmLabel: Label for the button that runs the action.
     * @action: Callback run when the user confirms.
     * @instance: Opaque pointer passed to @action.
     */
    void ShowConfirm(const char* title, const char* message, const char* confirmLabel,
                     MsgBoxAction action, void* instance);

    /** showDialog() - Make the dialog visible with the close button suppressed. */
    void showDialog();

    /** HideDialog() - Close the dialog and release the modal input lock. */
    void HideDialog();

    /** IsDialogOpen() - Return true while the dialog is on screen. */
    bool IsDialogOpen() const {
        return isVisible;
    }
};

#endif  // MESSAGEBOX_H
