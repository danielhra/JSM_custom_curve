#include "Gamepad.h"

size_t Gamepad::_count = 0;

Gamepad::Gamepad()
{
	++_count;
}

Gamepad::~Gamepad()
{
	--_count;
}

// Virtual controller output needs a driver that presents a HID device to the
// OS: ViGEm on Windows, uinput on Linux. macOS has no counterpart reachable
// from userspace - kexts are gone on Apple Silicon and a DriverKit HID driver
// needs an entitlement granted by Apple - so VIRTUAL_CONTROLLER stays
// unsupported here, exactly as it is on Linux in this tree.
Gamepad *Gamepad::getNew(ControllerScheme, Callback)
{
	return nullptr;
}
