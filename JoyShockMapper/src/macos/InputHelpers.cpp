#include "InputHelpers.h"

#include <ApplicationServices/ApplicationServices.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

// Command plumbing, consumed by the reader loop in main.cpp
std::queue<Command> commandQueue;
std::mutex commandQueueMutex;
std::condition_variable commandQueueCV;

namespace
{

// Carbon kVK_* virtual key codes, inlined rather than including the deprecated
// Carbon umbrella header (which also macro-pollutes C++ translation units).
constexpr CGKeyCode MK_A = 0x00, MK_S = 0x01, MK_D = 0x02, MK_F = 0x03, MK_H = 0x04,
                    MK_G = 0x05, MK_Z = 0x06, MK_X = 0x07, MK_C = 0x08, MK_V = 0x09,
                    MK_B = 0x0B, MK_Q = 0x0C, MK_W = 0x0D, MK_E = 0x0E, MK_R = 0x0F,
                    MK_Y = 0x10, MK_T = 0x11, MK_O = 0x1F, MK_U = 0x20, MK_I = 0x22,
                    MK_P = 0x23, MK_L = 0x25, MK_J = 0x26, MK_K = 0x28, MK_N = 0x2D,
                    MK_M = 0x2E;
constexpr CGKeyCode MK_1 = 0x12, MK_2 = 0x13, MK_3 = 0x14, MK_4 = 0x15, MK_5 = 0x17,
                    MK_6 = 0x16, MK_7 = 0x1A, MK_8 = 0x1C, MK_9 = 0x19, MK_0 = 0x1D;
constexpr CGKeyCode MK_EQUAL = 0x18, MK_MINUS = 0x1B, MK_RBRACKET = 0x1E,
                    MK_LBRACKET = 0x21, MK_QUOTE = 0x27, MK_SEMICOLON = 0x29,
                    MK_BACKSLASH = 0x2A, MK_COMMA = 0x2B, MK_SLASH = 0x2C,
                    MK_PERIOD = 0x2F, MK_GRAVE = 0x32;
constexpr CGKeyCode MK_KP0 = 0x52, MK_KP1 = 0x53, MK_KP2 = 0x54, MK_KP3 = 0x55,
                    MK_KP4 = 0x56, MK_KP5 = 0x57, MK_KP6 = 0x58, MK_KP7 = 0x59,
                    MK_KP8 = 0x5B, MK_KP9 = 0x5C, MK_KPDECIMAL = 0x41;
constexpr CGKeyCode MK_RETURN = 0x24, MK_TAB = 0x30, MK_SPACE = 0x31,
                    MK_BACKSPACE = 0x33, MK_ESCAPE = 0x35, MK_FORWARDDELETE = 0x75;
constexpr CGKeyCode MK_COMMAND = 0x37, MK_RCOMMAND = 0x36, MK_SHIFT = 0x38,
                    MK_RSHIFT = 0x3C, MK_OPTION = 0x3A, MK_ROPTION = 0x3D,
                    MK_CONTROL = 0x3B, MK_RCONTROL = 0x3E;
constexpr CGKeyCode MK_HOME = 0x73, MK_END = 0x77, MK_PAGEUP = 0x74,
                    MK_PAGEDOWN = 0x79, MK_LEFT = 0x7B, MK_RIGHT = 0x7C,
                    MK_DOWN = 0x7D, MK_UP = 0x7E, MK_MENU = 0x6E;
constexpr CGKeyCode MK_F1 = 0x7A, MK_F2 = 0x78, MK_F3 = 0x63, MK_F4 = 0x76,
                    MK_F5 = 0x60, MK_F6 = 0x61, MK_F7 = 0x62, MK_F8 = 0x64,
                    MK_F9 = 0x65, MK_F10 = 0x6D, MK_F11 = 0x67, MK_F12 = 0x6F,
                    MK_F13 = 0x69, MK_F14 = 0x6B, MK_F15 = 0x71, MK_F16 = 0x6A,
                    MK_F17 = 0x40, MK_F18 = 0x4F, MK_F19 = 0x50;

// 0x00 is a legitimate key code (A), so unmapped needs its own sentinel.
constexpr CGKeyCode MK_NONE = 0xFFFF;

constexpr CGKeyCode windows_key_to_mac_key(uint16_t key) noexcept
{
	switch (key)
	{
	case '0': return MK_0;
	case '1': return MK_1;
	case '2': return MK_2;
	case '3': return MK_3;
	case '4': return MK_4;
	case '5': return MK_5;
	case '6': return MK_6;
	case '7': return MK_7;
	case '8': return MK_8;
	case '9': return MK_9;
	case 'A': return MK_A;
	case 'B': return MK_B;
	case 'C': return MK_C;
	case 'D': return MK_D;
	case 'E': return MK_E;
	case 'F': return MK_F;
	case 'G': return MK_G;
	case 'H': return MK_H;
	case 'I': return MK_I;
	case 'J': return MK_J;
	case 'K': return MK_K;
	case 'L': return MK_L;
	case 'M': return MK_M;
	case 'N': return MK_N;
	case 'O': return MK_O;
	case 'P': return MK_P;
	case 'Q': return MK_Q;
	case 'R': return MK_R;
	case 'S': return MK_S;
	case 'T': return MK_T;
	case 'U': return MK_U;
	case 'V': return MK_V;
	case 'W': return MK_W;
	case 'X': return MK_X;
	case 'Y': return MK_Y;
	case 'Z': return MK_Z;
	case VK_OEM_PLUS: return MK_EQUAL;
	case VK_OEM_MINUS: return MK_MINUS;
	case VK_OEM_COMMA: return MK_COMMA;
	case VK_OEM_PERIOD: return MK_PERIOD;
	case VK_OEM_1: return MK_SEMICOLON;
	case VK_OEM_2: return MK_SLASH;
	case VK_OEM_3: return MK_GRAVE;
	case VK_OEM_4: return MK_LBRACKET;
	case VK_OEM_5: return MK_BACKSLASH;
	case VK_OEM_6: return MK_RBRACKET;
	case VK_OEM_7: return MK_QUOTE;
	case VK_F1: return MK_F1;
	case VK_F2: return MK_F2;
	case VK_F3: return MK_F3;
	case VK_F4: return MK_F4;
	case VK_F5: return MK_F5;
	case VK_F6: return MK_F6;
	case VK_F7: return MK_F7;
	case VK_F8: return MK_F8;
	case VK_F9: return MK_F9;
	case VK_F10: return MK_F10;
	case VK_F11: return MK_F11;
	case VK_F12: return MK_F12;
	case VK_F13: return MK_F13;
	case VK_F14: return MK_F14;
	case VK_F15: return MK_F15;
	case VK_F16: return MK_F16;
	case VK_F17: return MK_F17;
	case VK_F18: return MK_F18;
	case VK_F19: return MK_F19;
	case VK_NUMPAD0: return MK_KP0;
	case VK_NUMPAD1: return MK_KP1;
	case VK_NUMPAD2: return MK_KP2;
	case VK_NUMPAD3: return MK_KP3;
	case VK_NUMPAD4: return MK_KP4;
	case VK_NUMPAD5: return MK_KP5;
	case VK_NUMPAD6: return MK_KP6;
	case VK_NUMPAD7: return MK_KP7;
	case VK_NUMPAD8: return MK_KP8;
	case VK_NUMPAD9: return MK_KP9;
	case VK_BACK: return MK_BACKSPACE;
	case VK_LEFT: return MK_LEFT;
	case VK_RIGHT: return MK_RIGHT;
	case VK_UP: return MK_UP;
	case VK_DOWN: return MK_DOWN;
	case VK_SPACE: return MK_SPACE;
	case VK_CONTROL:
	case VK_LCONTROL: return MK_CONTROL;
	case VK_RCONTROL: return MK_RCONTROL;
	case VK_SHIFT:
	case VK_LSHIFT: return MK_SHIFT;
	case VK_RSHIFT: return MK_RSHIFT;
	// Windows' ALT is macOS' Option, in the same physical position.
	case VK_MENU:
	case VK_LMENU: return MK_OPTION;
	case VK_RMENU: return MK_ROPTION;
	// The Windows key sits where Command does.
	case VK_LWIN: return MK_COMMAND;
	case VK_RWIN: return MK_RCOMMAND;
	case VK_APPS: return MK_MENU;
	case VK_TAB: return MK_TAB;
	case VK_RETURN: return MK_RETURN;
	case VK_ESCAPE: return MK_ESCAPE;
	case VK_PRIOR: return MK_PAGEUP;
	case VK_NEXT: return MK_PAGEDOWN;
	case VK_HOME: return MK_HOME;
	case VK_END: return MK_END;
	case VK_DELETE: return MK_FORWARDDELETE;
	default: return MK_NONE;
	}
}

// Modifier state has to be stamped onto every synthesised event: posting a
// bare Shift key-down does not make the window server treat later events as
// shifted, so JSM has to carry the flags itself.
CGEventFlags modifier_flag_for(CGKeyCode key) noexcept
{
	switch (key)
	{
	case MK_SHIFT: return kCGEventFlagMaskShift;
	case MK_RSHIFT: return kCGEventFlagMaskShift;
	case MK_CONTROL: return kCGEventFlagMaskControl;
	case MK_RCONTROL: return kCGEventFlagMaskControl;
	case MK_OPTION: return kCGEventFlagMaskAlternate;
	case MK_ROPTION: return kCGEventFlagMaskAlternate;
	case MK_COMMAND: return kCGEventFlagMaskCommand;
	case MK_RCOMMAND: return kCGEventFlagMaskCommand;
	default: return 0;
	}
}

class VirtualInput
{
public:
	VirtualInput()
	{
		source_ = CGEventSourceCreate(kCGEventSourceStateHIDSystemState);
		if (source_)
		{
			// Without this, every synthesised event freezes the user's real
			// mouse for a quarter second.
			CGEventSourceSetLocalEventsSuppressionInterval(source_, 0.0);
		}
	}

	~VirtualInput()
	{
		if (source_)
			CFRelease(source_);
	}

	void press_key(CGKeyCode key, bool pressed)
	{
		if (const auto flag = modifier_flag_for(key); flag != 0)
		{
			if (pressed)
				flags_ |= flag;
			else
				flags_ &= ~flag;
		}

		CGEventRef event = CGEventCreateKeyboardEvent(source_, key, pressed);
		if (!event)
			return;
		CGEventSetFlags(event, flags_);
		CGEventPost(kCGHIDEventTap, event);
		CFRelease(event);
	}

	void press_mouse_button(CGMouseButton button, bool pressed)
	{
		CGEventType type = kCGEventNull;
		switch (button)
		{
		case kCGMouseButtonLeft:
			type = pressed ? kCGEventLeftMouseDown : kCGEventLeftMouseUp;
			break;
		case kCGMouseButtonRight:
			type = pressed ? kCGEventRightMouseDown : kCGEventRightMouseUp;
			break;
		default:
			type = pressed ? kCGEventOtherMouseDown : kCGEventOtherMouseUp;
			break;
		}

		if (pressed)
			buttons_ |= (1u << button);
		else
			buttons_ &= ~(1u << button);

		CGEventRef event = CGEventCreateMouseEvent(source_, type, cursor(), button);
		if (!event)
			return;
		CGEventSetFlags(event, flags_);
		CGEventSetIntegerValueField(event, kCGMouseEventClickState, 1);
		CGEventPost(kCGHIDEventTap, event);
		CFRelease(event);
	}

	// Relative motion. Measured on macOS 15/SDL3: stamping the delta fields
	// and leaving the cursor alone is the only form a relative-mouse-mode
	// game receives 1:1. Also moving the cursor makes SDL add its warp
	// correction on top of the delta, inflating motion unpredictably.
	void move_relative(int dx, int dy)
	{
		if (dx == 0 && dy == 0)
			return;

		CGEventRef event = CGEventCreateMouseEvent(source_, dragged_or_moved(), cursor(), held_button());
		if (!event)
			return;
		CGEventSetIntegerValueField(event, kCGMouseEventDeltaX, dx);
		CGEventSetIntegerValueField(event, kCGMouseEventDeltaY, dy);
		CGEventSetFlags(event, flags_);
		CGEventPost(kCGHIDEventTap, event);
		CFRelease(event);
	}

	// Absolute positioning does move the cursor, and measured exactly 1:1.
	void move_absolute(double x, double y)
	{
		CGEventRef event = CGEventCreateMouseEvent(source_, dragged_or_moved(),
		                                           CGPointMake(x, y), held_button());
		if (event)
		{
			CGEventSetFlags(event, flags_);
			CGEventPost(kCGHIDEventTap, event);
			CFRelease(event);
		}
		CGWarpMouseCursorPosition(CGPointMake(x, y));
	}

	void scroll(int amount)
	{
		CGEventRef event = CGEventCreateScrollWheelEvent(source_, kCGScrollEventUnitLine, 1, amount);
		if (!event)
			return;
		CGEventSetFlags(event, flags_);
		CGEventPost(kCGHIDEventTap, event);
		CFRelease(event);
	}

private:
	static CGPoint cursor()
	{
		CGEventRef probe = CGEventCreate(nullptr);
		if (!probe)
			return CGPointMake(0, 0);
		const CGPoint here = CGEventGetLocation(probe);
		CFRelease(probe);
		return here;
	}

	// A move while a button is held has to be reported as a drag, or apps
	// treating press-move-release as a gesture will not follow it.
	CGEventType dragged_or_moved() const noexcept
	{
		if (buttons_ & (1u << kCGMouseButtonLeft))
			return kCGEventLeftMouseDragged;
		if (buttons_ & (1u << kCGMouseButtonRight))
			return kCGEventRightMouseDragged;
		if (buttons_ != 0)
			return kCGEventOtherMouseDragged;
		return kCGEventMouseMoved;
	}

	CGMouseButton held_button() const noexcept
	{
		if (buttons_ & (1u << kCGMouseButtonLeft))
			return kCGMouseButtonLeft;
		if (buttons_ & (1u << kCGMouseButtonRight))
			return kCGMouseButtonRight;
		return kCGMouseButtonLeft;
	}

	CGEventSourceRef source_ = nullptr;
	CGEventFlags flags_ = 0;
	uint32_t buttons_ = 0;
};

VirtualInput &input()
{
	static VirtualInput instance;
	return instance;
}

} // namespace

float getMouseSpeed()
{
	// Nothing in the CGEvent path applies a pointer-acceleration curve to what
	// JSM posts, so there is no OS sensitivity to divide out.
	return 1.0f;
}

int pressMouse(KeyCode vkKey, bool isPressed)
{
	switch (vkKey.code)
	{
	case V_WHEEL_UP:
		if (isPressed)
			input().scroll(1);
		return 0;
	case V_WHEEL_DOWN:
		if (isPressed)
			input().scroll(-1);
		return 0;
	case VK_LBUTTON:
		input().press_mouse_button(kCGMouseButtonLeft, isPressed);
		return 0;
	case VK_RBUTTON:
		input().press_mouse_button(kCGMouseButtonRight, isPressed);
		return 0;
	case VK_MBUTTON:
		input().press_mouse_button(kCGMouseButtonCenter, isPressed);
		return 0;
	case VK_XBUTTON1:
		input().press_mouse_button(static_cast<CGMouseButton>(3), isPressed);
		return 0;
	case VK_XBUTTON2:
		input().press_mouse_button(static_cast<CGMouseButton>(4), isPressed);
		return 0;
	default:
		return 0;
	}
}

int pressKey(KeyCode vkKey, bool pressed)
{
	if (vkKey.code == 0)
		return 0;
	if (vkKey.code <= V_WHEEL_DOWN) // Highest mouse ID
		return pressMouse(vkKey, pressed);

	const auto key = windows_key_to_mac_key(vkKey.code);
	if (key == MK_NONE)
		return 0;

	input().press_key(key, pressed);
	return 0;
}

float accumulatedX = 0;
float accumulatedY = 0;

void moveMouse(float x, float y)
{
	accumulatedX += x;
	accumulatedY += y;

	int applicableX = (int)accumulatedX;
	int applicableY = (int)accumulatedY;

	accumulatedX -= applicableX;
	accumulatedY -= applicableY;

	input().move_relative(applicableX, applicableY);
}

void setMouseNorm(float x, float y)
{
	const auto display = CGMainDisplayID();
	const double width = CGDisplayPixelsWide(display);
	const double height = CGDisplayPixelsHigh(display);
	input().move_absolute(x * width, y * height);
}

bool WriteToConsole(string_view command)
{
	std::lock_guard<std::mutex> lock(commandQueueMutex);
	commandQueue.push(Command{ std::string(command), CommandSource::INTERNAL });
	commandQueueCV.notify_one();
	return true;
}

BOOL ConsoleCtrlHandler(DWORD)
{
	return false;
}

void initConsole(std::function<void()>)
{
	// Detached on purpose: a static std::thread is still joinable when static
	// destructors run at exit, and ~thread() on a joinable thread aborts.
	static std::once_flag once;
	std::call_once(once, [] {
		std::thread([] {
			std::string line;
			while (std::getline(std::cin, line))
			{
				std::lock_guard<std::mutex> lock(commandQueueMutex);
				commandQueue.push(Command{ line, CommandSource::CONSOLE });
				commandQueueCV.notify_one();
			}
		}).detach();
	});
}

void initConsole()
{
	static std::once_flag once;
	std::call_once(once, [] {
		std::thread([] {
		FILE *tty = fopen("/dev/tty", "r");
		if (!tty)
		{
			perror("fopen /dev/tty");
			return;
		}
		char *lineptr = nullptr;
		size_t n = 0;
		while (true)
		{
			const ssize_t read = getline(&lineptr, &n, tty);
			if (read == -1)
				break;

			std::lock_guard<std::mutex> lock(commandQueueMutex);
			commandQueue.push(Command{ std::string(lineptr, read), CommandSource::CONSOLE });
			commandQueueCV.notify_one();
		}
		free(lineptr);
		fclose(tty);
		}).detach();
	});
}

void initFifoCommandListener()
{
	const char *fifo_path = "/tmp/jsm_command_fifo";
	if (access(fifo_path, F_OK) == -1)
	{
		if (mkfifo(fifo_path, 0666) != 0)
		{
			perror("mkfifo");
			return;
		}
	}

	std::thread([]() {
		const char *fifo_path = "/tmp/jsm_command_fifo";

		const int fifo_read_fd = open(fifo_path, O_RDONLY | O_NONBLOCK);
		if (fifo_read_fd < 0)
		{
			perror("open fifo for reading");
			return;
		}

		// Keep a writer open so the reader never sees EOF.
		const int fifo_write_fd = open(fifo_path, O_WRONLY);

		FILE *fifo_file = fdopen(fifo_read_fd, "r");
		if (!fifo_file)
		{
			perror("fdopen fifo");
			close(fifo_read_fd);
			if (fifo_write_fd >= 0)
				close(fifo_write_fd);
			return;
		}

		while (true)
		{
			char *lineptr = nullptr;
			size_t n = 0;
			const ssize_t read_len = getline(&lineptr, &n, fifo_file);
			if (read_len == -1)
			{
				free(lineptr);
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				clearerr(fifo_file);
				continue;
			}

			std::string line(lineptr, read_len);
			free(lineptr);

			if (!line.empty() && line.back() == '\n')
				line.pop_back();

			std::lock_guard<std::mutex> lock(commandQueueMutex);
			commandQueue.push(Command{ line, CommandSource::FIFO });
			commandQueueCV.notify_one();
		}
	}).detach();
}

// Returns (application name, window title). The app name comes from the
// window list and needs no special permission; the title is only populated
// when the user has granted Screen Recording, so AutoLoad rules that match
// on the app name keep working either way.
std::tuple<std::string, std::string> GetActiveWindowName()
{
	std::tuple<std::string, std::string> result;

	CFArrayRef windows = CGWindowListCopyWindowInfo(
	  kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements, kCGNullWindowID);
	if (windows == nullptr)
		return result;

	const auto copyString = [](CFDictionaryRef dict, CFStringRef key, std::string &out) {
		const auto value = static_cast<CFStringRef>(CFDictionaryGetValue(dict, key));
		if (value == nullptr)
			return;
		const CFIndex size = CFStringGetMaximumSizeForEncoding(CFStringGetLength(value), kCFStringEncodingUTF8) + 1;
		std::string buffer(static_cast<size_t>(size), '\0');
		if (CFStringGetCString(value, buffer.data(), size, kCFStringEncodingUTF8))
			out = buffer.c_str();
	};

	// The list comes back front to back, so the first layer-0 window is the
	// frontmost one the user is actually looking at.
	const CFIndex count = CFArrayGetCount(windows);
	for (CFIndex i = 0; i < count; ++i)
	{
		const auto window = static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(windows, i));
		if (window == nullptr)
			continue;

		int layer = -1;
		const auto layerRef = static_cast<CFNumberRef>(CFDictionaryGetValue(window, kCGWindowLayer));
		if (layerRef == nullptr || !CFNumberGetValue(layerRef, kCFNumberIntType, &layer) || layer != 0)
			continue;

		copyString(window, kCGWindowOwnerName, std::get<0>(result));
		copyString(window, kCGWindowName, std::get<1>(result));
		break;
	}

	CFRelease(windows);
	return result;
}

std::vector<std::string> ListDirectory(std::string directory)
{
	std::vector<std::string> fileListing;

	dirent **entries = nullptr;
	const auto entryCount = ::scandir(directory.c_str(), &entries, nullptr, &::alphasort);
	if (entryCount <= 0)
		return fileListing;

	fileListing.reserve(entryCount);
	for (int i = 0; i < entryCount; ++i)
	{
		if (entries[i]->d_type == DT_REG)
			fileListing.emplace_back(entries[i]->d_name);
		free(entries[i]);
	}
	free(entries);

	return fileListing;
}

std::string GetCWD()
{
	std::unique_ptr<char, decltype(&std::free)> pathBuffer{ getcwd(nullptr, 0), &std::free };
	return pathBuffer ? pathBuffer.get() : std::string{};
}

bool SetCWD(string_view newCWD)
{
	return chdir(std::string(newCWD).c_str()) != 0;
}

DWORD ShowOnlineHelp()
{
	::system("open https://github.com/JibbSmart/JoyShockMapper/blob/master/README.md");
	return 0;
}

void HideConsole()
{
}

void UnhideConsole()
{
}

void ShowConsole()
{
}

void ReleaseConsole()
{
}

bool IsVisible()
{
	return true;
}

bool isConsoleMinimized()
{
	return false;
}

bool ClearConsole()
{
	return true;
}
