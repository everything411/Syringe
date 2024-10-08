#pragma once
#define WIN32_LEAN_AND_MEAN
//      WIN32_FAT_AND_STUPID

#include "CRC32.h"
#include "PortableExecutable.h"

#include <cstring>
#include <iostream>
#include <map>
#include <optional>
#include <string_view>

#include <windows.h>

#include <share.h>

#define __FUNCTION__

class SyringeDebugger
{
	static constexpr size_t MaxNameLength = 0x100u;

	static constexpr BYTE INIT = 0x00;
	static constexpr BYTE INT3 = 0xCC; // trap to debugger interrupt opcode.
	static constexpr BYTE NOP = 0x90;

public:
	SyringeDebugger(std::string_view filename)
		: exe(filename)
	{
		RetrieveInfo();
	}

	// debugger
	void Run(std::string_view arguments);
	DWORD HandleException(DEBUG_EVENT const& dbgEvent);

	// breakpoints
	bool SetBP(void* address);
	void RemoveBP(LPVOID address, bool restoreOpcode);

	// memory
	VirtualMemoryHandle AllocMem(void* address, size_t size);
	bool PatchMem(void* address, void const* buffer, DWORD size);
	bool ReadMem(void const* address, void* buffer, DWORD size);

	// syringe
	void FindDLLs();

private:
	void RetrieveInfo();
	void DebugProcess(std::string_view arguments);

	// helper Functions
	static DWORD __fastcall RelativeOffset(void const* from, void const* to);

	template<typename T>
	static void ApplyPatch(void* ptr, T&& data) noexcept
	{
		std::memcpy(ptr, &data, sizeof(data));
	}

	// thread info
	struct ThreadInfo
	{
		ThreadInfo() = default;

		ThreadInfo(HANDLE hThread) noexcept
			: Thread{hThread}
		{ }

		ThreadHandle Thread;
		LPVOID lastBP{ nullptr };
	};

	std::map<DWORD, ThreadInfo> Threads;

	// process info
	PROCESS_INFORMATION pInfo;

	// flags
	bool bEntryBP{ true };

	// breakpoints
	struct Hook
	{
		char lib[MaxNameLength];
		char proc[MaxNameLength];
		void* proc_address;

		size_t num_overridden;
	};

	struct BreakpointInfo
	{
		BYTE original_opcode{ 0x0u };
		std::vector<Hook> hooks;
		VirtualMemoryHandle p_caller_code;
	};

	std::map<void*, BreakpointInfo> Breakpoints;

	std::vector<Hook*> v_AllHooks;
	std::vector<Hook*>::iterator loop_LoadLibrary;

	// syringe
	std::string exe;
	void* pcEntryPoint{ nullptr };
	void* pImLoadLibrary{ nullptr };
	void* pImGetProcAddress{ nullptr };
	VirtualMemoryHandle pAlloc;
	DWORD dwTimeStamp{ 0u };
	DWORD dwExeSize{ 0u };
	DWORD dwExeCRC{ 0u };

	bool bDLLsLoaded{ false };
	bool bHooksCreated{ false };

	bool bAVLogged{ false };

	// data addresses
	struct AllocData {
		static constexpr auto CodeSize = 0x40u;
		std::byte LoadLibraryFunc[CodeSize];
		void* ProcAddress;
		void* ReturnEIP;
		char LibName[MaxNameLength];
		char ProcName[MaxNameLength];
	};

	AllocData* GetData() const noexcept {
		return reinterpret_cast<AllocData*>(pAlloc.get());
	};

	struct HookBuffer {
		std::map<void*, std::vector<Hook>> hooks;
		CRC32 checksum;
		size_t count{ 0 };

		void add(void* const eip, Hook const& hook) {
			auto& h = hooks[eip];
			h.push_back(hook);

			checksum.compute(&eip, sizeof(eip));
			checksum.compute(&hook.num_overridden, sizeof(hook.num_overridden));
			count++;
		}

		void add(
			void* const eip, std::string_view const filename,
			std::string_view const proc, size_t const num_overridden)
		{
			Hook hook;
			hook.lib[filename.copy(hook.lib, std::size(hook.lib) - 1)] = '\0';
			hook.proc[proc.copy(hook.proc, std::size(hook.proc) - 1)] = '\0';
			hook.proc_address = nullptr;
			hook.num_overridden = num_overridden;

			add(eip, hook);
		}
	};

	bool ParseInjFileHooks(std::string_view lib, HookBuffer& hooks);
	bool CanHostDLL(PortableExecutable const& DLL, IMAGE_SECTION_HEADER const& hosts) const;
	bool ParseHooksSection(PortableExecutable const& DLL, IMAGE_SECTION_HEADER const& hooks, HookBuffer& buffer);
	std::optional<bool> Handshake(char const* lib, int hooks, unsigned int crc);
};

// disable "structures padded due to alignment specifier"
#pragma warning(push)
#pragma warning(disable : 4324)
struct alignas(16) hookdecl {
	unsigned int hookAddr;
	unsigned int hookSize;
	DWORD hookNamePtr;
};

struct alignas(16) hostdecl {
	unsigned int hostChecksum;
	DWORD hostNamePtr;
};

static_assert(sizeof(hookdecl) == 16);
static_assert(sizeof(hostdecl) == 16);
#pragma warning(pop)

struct SyringeHandshakeInfo
{
	int cbSize;
	int num_hooks;
	unsigned int checksum;
	DWORD exeFilesize;
	DWORD exeTimestamp;
	unsigned int exeCRC;
	int cchMessage;
	char* Message;
};

using SYRINGEHANDSHAKEFUNC = HRESULT(__cdecl *)(SyringeHandshakeInfo*);
