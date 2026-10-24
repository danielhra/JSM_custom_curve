#include "Whitelister.h"

// Whitelisting exists to hide a physical controller from games on Windows so
// they do not see both it and JSM's virtual pad. macOS has no virtual pad and
// no HidHide equivalent, so there is nothing to hide from.
Whitelister *Whitelister::getNew(bool)
{
	return nullptr;
}
