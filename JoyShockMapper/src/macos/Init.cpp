#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

#include <mach-o/dyld.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "InputHelpers.h"

// A signal handler may only call async-signal-safe functions, and
// WriteToConsole locks a mutex, allocates a string and signals a condition
// variable. So the handler does nothing but hand the signal to a normal
// thread through a pipe, which write() is safe to do.
static int signalPipe[2] = { -1, -1 };

static void terminateHandler(int signo)
{
	if (signalPipe[1] < 0)
		return;

	const char token = static_cast<char>(signo);
	const ssize_t ignored = ::write(signalPipe[1], &token, 1);
	(void)ignored;
}

static void startSignalForwarder()
{
	if (::pipe(signalPipe) != 0)
		return;

	std::thread([] {
		char token = 0;
		while (::read(signalPipe[0], &token, 1) == 1)
		{
			if (token != SIGTERM && token != SIGINT)
			{
				std::fprintf(stderr, "Caught unexpected signal %d\n", int(token));
				continue;
			}

			// Back on an ordinary thread, so locking and allocating is fine.
			WriteToConsole("QUIT");
		}
	}).detach();

	signal(SIGTERM, &terminateHandler);
	signal(SIGINT, &terminateHandler);
}

// Where the bundled GyroConfigs were installed, relative to the binary.
static std::filesystem::path globalConfigDirectory()
{
	uint32_t size = 0;
	_NSGetExecutablePath(nullptr, &size);
	std::string buffer(size, '\0');
	if (_NSGetExecutablePath(buffer.data(), &size) != 0)
		return {};

	std::error_code ec;
	auto exePath = std::filesystem::canonical(std::filesystem::path(buffer.c_str()), ec);
	if (ec)
		return {};

	return exePath.parent_path() / ".." / "etc" / "JoyShockMapper" / "GyroConfigs";
}

static const int initialize = [] {
	// The GUI launches JSM with its stdout on a pipe, which stdio would
	// block-buffer; flush as we go so the UI sees replies promptly. A real
	// terminal is already line-buffered, so leave that case alone.
	if (!isatty(STDOUT_FILENO))
		std::cout.setf(std::ios::unitbuf);

	std::error_code ec;

	const std::filesystem::path configDirectory{ BASE_JSM_CONFIG_FOLDER() };
	std::filesystem::create_directories(configDirectory / "AutoLoad", ec);
	std::filesystem::create_directories(configDirectory / "GyroConfigs", ec);

	// Seed the user's GyroConfigs from the installed copies, without ever
	// clobbering a file they have edited.
	const auto source = globalConfigDirectory();
	if (!source.empty() && std::filesystem::is_directory(source, ec))
	{
		const auto destination = configDirectory / "GyroConfigs";
		for (const auto &entry : std::filesystem::directory_iterator(source, ec))
		{
			if (!entry.is_regular_file())
				continue;

			const auto target = destination / entry.path().filename();
			if (std::filesystem::exists(target, ec))
				continue;

			std::filesystem::copy_file(entry.path(), target, ec);
		}
	}

	startSignalForwarder();

	return 0;
}();
