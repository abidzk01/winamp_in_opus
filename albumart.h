#ifndef ALBUMART_H
#define ALBUMART_H

#include <windows.h>

/* Registers a Wasabi album art provider with Winamp's service manager, so that
 * the cover art embedded in an Opus file shows up in Winamp's own Album Art
 * window and in the artwork pane of the File Info dialog. Safe to call once,
 * from the plug-in's init(); it does nothing when the service manager is not
 * available (Winamp older than 5.12). */
void albumart_register(HWND winamp);

/* Removes the provider again, called from the plug-in's quit(). */
void albumart_unregister(void);

#endif