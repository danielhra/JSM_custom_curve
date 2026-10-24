#include "TrayIcon.h"

// JSM runs headless on macOS for now. main.cpp treats a null tray as "no tray
// available" and keeps every command reachable from the console, so this is a
// missing convenience rather than a missing feature. A real implementation
// would wrap NSStatusItem in an Objective-C++ translation unit; the TrayIcon
// interface maps onto it directly.
TrayIcon *TrayIcon::getNew(TrayIconData, std::function<void()> &&)
{
	return nullptr;
}
