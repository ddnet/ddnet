#ifndef ENGINE_CLIENT_X11_ERROR_HANDLER_H
#define ENGINE_CLIENT_X11_ERROR_HANDLER_H

// Keeps Xlib from ending the process when the X server rejects a RandR output that no
// longer exists. Does nothing unless SDL is using its X11 video driver, so this must be
// called after the video subsystem was initialized. Repeated calls have no effect.
void X11IgnoreStaleOutputErrors();

#endif
