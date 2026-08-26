#include "Memory.hpp"
#ifdef _WIN32
#include <tlhelp32.h>

uint32_t pProcess::FindProcessIdByProcessName(const char* ProcessName)
{
	std::wstring wideProcessName;
	int wideCharLength = MultiByteToWideChar(CP_UTF8, 0, ProcessName, -1, nullptr, 0);
	if (wideCharLength > 0)
	{
		wideProcessName.resize(wideCharLength);
		MultiByteToWideChar(CP_UTF8, 0, ProcessName, -1, &wideProcessName[0], wideCharLength);
	}

	HANDLE hPID = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
	PROCESSENTRY32W process_entry_{ };
	process_entry_.dwSize = sizeof(PROCESSENTRY32W);

	DWORD pid = 0;
	if (Process32FirstW(hPID, &process_entry_))
	{
		do
		{
			if (!wcscmp(process_entry_.szExeFile, wideProcessName.c_str()))
			{
				pid = process_entry_.th32ProcessID;
				break;
			}
		} while (Process32NextW(hPID, &process_entry_));
	}
	CloseHandle(hPID);
	return pid;
}

uint32_t pProcess::FindProcessIdByWindowName(const char* WindowName)
{
	DWORD process_id = 0;
	HWND windowHandle = FindWindowA(nullptr, WindowName);
	if (windowHandle)
		GetWindowThreadProcessId(windowHandle, &process_id);
	return process_id;
}

HWND pProcess::GetWindowHandleFromProcessId(DWORD ProcessId) {
	HWND hwnd = NULL;
	do {
		hwnd = FindWindowEx(NULL, hwnd, NULL, NULL);
		DWORD pid = 0;
		GetWindowThreadProcessId(hwnd, &pid);
		if (pid == ProcessId) {
			TCHAR windowTitle[MAX_PATH];
			GetWindowText(hwnd, windowTitle, MAX_PATH);
			if (IsWindowVisible(hwnd) && windowTitle[0] != '\0') {
				return hwnd;
			}
		}
	} while (hwnd != NULL);
	return NULL; // No main window found for the given process ID
}

  bool pProcess::AttachProcess(const char* ProcessName)
{
	this->pid_ = this->FindProcessIdByProcessName(ProcessName);

	if (pid_)
	{
		HMODULE modules[0xFF];
		MODULEINFO module_info;
		DWORD _;

		handle_ = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION |
                                PROCESS_VM_READ, FALSE, pid_);

		if (!handle_)
			return false;

		EnumProcessModulesEx(this->handle_, modules, sizeof(modules), &_, LIST_MODULES_64BIT);
		base_client_.base = (uintptr_t)modules[0];

		GetModuleInformation(this->handle_, modules[0], &module_info, sizeof(module_info));
		base_client_.size = module_info.SizeOfImage;

		hwnd_ = this->GetWindowHandleFromProcessId(pid_);

		return true;
	}

	return false;
}

bool pProcess::AttachWindow(const char* WindowName)
{
	this->pid_ = this->FindProcessIdByWindowName(WindowName);

	if (pid_)
	{
		HMODULE modules[0xFF];
		MODULEINFO module_info;
		DWORD _;

		handle_ = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid_);

		EnumProcessModulesEx(this->handle_, modules, sizeof(modules), &_, LIST_MODULES_64BIT);
		base_client_.base = (uintptr_t)modules[0];

		GetModuleInformation(this->handle_, modules[0], &module_info, sizeof(module_info));
		base_client_.size = module_info.SizeOfImage;

		hwnd_ = this->GetWindowHandleFromProcessId(pid_);

		return true;
	}
	return false;
}

bool pProcess::UpdateHWND()
{
	hwnd_ = this->GetWindowHandleFromProcessId(pid_);
	return hwnd_ != nullptr;
}

ProcessModule pProcess::GetModule(const char* lModule)
{
	std::wstring wideModule;
	int wideCharLength = MultiByteToWideChar(CP_UTF8, 0, lModule, -1, nullptr, 0);
	if (wideCharLength > 0)
	{
		wideModule.resize(wideCharLength);
		MultiByteToWideChar(CP_UTF8, 0, lModule, -1, &wideModule[0], wideCharLength);
	}

	HANDLE handle_module = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid_);
	MODULEENTRY32W module_entry_{};
	module_entry_.dwSize = sizeof(MODULEENTRY32W);

	do
	{
		if (!wcscmp(module_entry_.szModule, wideModule.c_str()))
		{
			CloseHandle(handle_module);
			return { (DWORD_PTR)module_entry_.modBaseAddr, module_entry_.dwSize };
		}
	} while (Module32NextW(handle_module, &module_entry_));

	CloseHandle(handle_module);
	return { 0, 0 };
}

LPVOID pProcess::Allocate(size_t size_in_bytes)
{
	return VirtualAllocEx(this->handle_, NULL, size_in_bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}

uintptr_t pProcess::FindSignature(std::vector<uint8_t> signature)
{
	std::unique_ptr<uint8_t[]> data;
	data = std::make_unique<uint8_t[]>(this->base_client_.size);

	if (!ReadProcessMemory(this->handle_, (void*)(this->base_client_.base), data.get(), this->base_client_.size, NULL)) {
		return 0x0;
	}

	for (uintptr_t i = 0; i < this->base_client_.size; i++)
	{
		for (uintptr_t j = 0; j < signature.size(); j++)
		{
			if (signature.at(j) == 0x00)
				continue;

			if (*reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(&data[i + j])) == signature.at(j))
			{
				if (j == signature.size() - 1)
					return this->base_client_.base + i;
				continue;
			}
			break;
		}
	}
	return 0x0;
}

uintptr_t pProcess::FindSignature(ProcessModule target_module, std::vector<uint8_t> signature)
{
	std::unique_ptr<uint8_t[]> data;
	data = std::make_unique<uint8_t[]>(0xFFFFFFF);

	if (!ReadProcessMemory(this->handle_, (void*)(target_module.base), data.get(), 0xFFFFFFF, NULL)) {
		return NULL;
	}

	for (uintptr_t i = 0; i < 0xFFFFFFF; i++)
	{
		for (uintptr_t j = 0; j < signature.size(); j++)
		{
			if (signature.at(j) == 0x00)
				continue;

			if (*reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(&data[i + j])) == signature.at(j))
			{
				if (j == signature.size() - 1)
					return this->base_client_.base + i;
				continue;
			}
			break;
		}
	}
	return 0x0;
}

uintptr_t pProcess::FindCodeCave(uint32_t length_in_bytes)
{
	std::vector<uint8_t> cave_pattern = {};

	for (uint32_t i = 0; i < length_in_bytes; i++) {
		cave_pattern.push_back(0x00);
	}

	return FindSignature(cave_pattern);
}

void pProcess::Close()
{
	CloseHandle(handle_);
}
#else
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string_view>
#include <sys/uio.h>
#include <unistd.h>

namespace {
std::string basename_of(std::string path) {
    const auto slash = path.find_last_of('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}
}

uint32_t pProcess::FindProcessIdByProcessName(const char* process_name) {
    const std::string wanted = process_name;
    const std::string wanted_base = basename_of(wanted);
    for (const auto& entry : std::filesystem::directory_iterator("/proc")) {
        const auto name = entry.path().filename().string();
        if (name.empty() || !std::all_of(name.begin(), name.end(), ::isdigit)) continue;
        std::ifstream comm(entry.path() / "comm");
        std::string actual;
        std::getline(comm, actual);
        if (actual == wanted || actual == wanted_base ||
            (wanted == "cs2.exe" && actual == "cs2"))
            return static_cast<uint32_t>(std::stoul(name));
    }
    return 0;
}

bool pProcess::AttachProcess(const char* process_name) {
    pid_ = static_cast<pid_t>(FindProcessIdByProcessName(process_name));
    if (!pid_) return false;
    handle_ = 1;
    base_client_ = GetModule("libclient.so");
    if (!base_client_.base) base_client_ = GetModule("client.so");
    return true;
}

bool pProcess::AttachWindow(const char*) { return false; }
bool pProcess::UpdateHWND() { return pid_ > 0 && std::filesystem::exists("/proc/" + std::to_string(pid_)); }

ProcessModule pProcess::GetModule(const char* module_name) {
    if (!pid_) return {};
    std::ifstream maps("/proc/" + std::to_string(pid_) + "/maps");
    std::string line;
    uintptr_t first = std::numeric_limits<uintptr_t>::max(), last = 0;
    while (std::getline(maps, line)) {
        const auto path_pos = line.find('/');
        if (path_pos == std::string::npos) continue;
        const std::string path = line.substr(path_pos);
        if (basename_of(path) != module_name) continue;
        uintptr_t begin{}, end{};
        if (std::sscanf(line.c_str(), "%lx-%lx", &begin, &end) != 2) continue;
        first = std::min(first, begin);
        last = std::max(last, end);
    }
    if (!last) return {};
    return {first, last - first};
}

bool pProcess::read_raw_linux(uintptr_t address, void* buffer, size_t size) const {
    iovec local{buffer, size};
    iovec remote{reinterpret_cast<void*>(address), size};
    return process_vm_readv(pid_, &local, 1, &remote, 1, 0) == static_cast<ssize_t>(size);
}

bool pProcess::write_raw_linux(uintptr_t address, const void* buffer, size_t size) const {
    iovec local{const_cast<void*>(buffer), size};
    iovec remote{reinterpret_cast<void*>(address), size};
    return process_vm_writev(pid_, &local, 1, &remote, 1, 0) == static_cast<ssize_t>(size);
}

LPVOID pProcess::Allocate(size_t) { return nullptr; }

uintptr_t pProcess::FindSignature(std::vector<uint8_t> signature) {
    return FindSignature(base_client_, std::move(signature));
}

uintptr_t pProcess::FindSignature(ProcessModule module, std::vector<uint8_t> signature) {
    if (!module.base || !module.size || signature.empty()) return 0;
    constexpr size_t chunk_size = 1024 * 1024;
    std::vector<uint8_t> data(chunk_size + signature.size());
    for (uintptr_t offset = 0; offset < module.size; offset += chunk_size) {
        const auto amount = std::min<size_t>(data.size(), module.size - offset);
        if (!read_raw(module.base + offset, data.data(), amount)) continue;
        for (size_t i = 0; i + signature.size() <= amount; ++i) {
            bool match = true;
            for (size_t j = 0; j < signature.size(); ++j)
                if (signature[j] && signature[j] != data[i + j]) { match = false; break; }
            if (match) return module.base + offset + i;
        }
    }
    return 0;
}

uintptr_t pProcess::FindCodeCave(uint32_t length) { return FindSignature(std::vector<uint8_t>(length)); }
void pProcess::Close() { handle_ = 0; pid_ = 0; }
#endif
