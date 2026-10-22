#pragma once

#include <functional>

// What the UI can ask of the OS window. Implemented in main.cpp.
// Calls are safe from inside an ImGui frame: anything that would resize the
// window, reset the D3D device or open a modal dialog is deferred until the
// frame has been presented.
namespace Window
{
    void  Resize(float width, float height); // size in 96-DPI units; the window is re-centered on its monitor
    void  StartDrag();                       // move the window with the mouse until the left button is released
    void  Minimize();
    void  Restore();                         // un-minimize and bring to the front
    void  Close();
    bool  HasRoundedCorners();               // true when Windows 11 rounds the corners for us
    void* NativeHandle();                    // the HWND, e.g. to own a dialog
    void  RunAfterFrame(std::function<void()> fn); // e.g. modal dialogs, which run their own message loop
}
