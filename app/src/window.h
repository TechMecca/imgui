#pragma once

// What the UI can ask of the OS window. Implemented in main.cpp.
// Calls are safe from inside an ImGui frame: anything that would resize the
// window or reset the D3D device is deferred until the frame has been presented.
namespace Window
{
    void  Resize(float width, float height); // size in 96-DPI units; the window is re-centered on its monitor
    void  StartDrag();                       // move the window with the mouse until the left button is released
    void  Minimize();
    void  Close();
    bool  HasRoundedCorners();               // true when Windows 11 rounds the corners for us
}
