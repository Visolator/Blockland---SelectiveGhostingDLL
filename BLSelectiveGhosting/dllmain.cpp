#include "torque.hpp"
#include "detours\detours.h"

BLFUNC(void, __thiscall, NetConnection__detachObject, DWORD conn, DWORD obj)
BLFUNC(void, __thiscall, NetConnection__clearLocalScopeAlways, DWORD conn, DWORD obj)
MologieDetours::Detour<NetConnection__clearLocalScopeAlwaysFn>* Detour__clearLocalScopeAlways = NULL;

void __fastcall Hooked__clearLocalScopeAlways(DWORD conn, void* blank, DWORD obj)
{
	if (!*(DWORD*)(conn + 492))
		return;

	// Who knows if this can change?
	DWORD hash = 4 * (*(DWORD*)(obj + 32) & (*(DWORD*)0x0071FB6C - 1));

	for (DWORD ref = *(DWORD*)(*(DWORD*)(conn + 524) + hash); ref; ref = *(DWORD*)(ref + 32))
	{
		if (*(DWORD*)ref != obj)
			continue;

		if (*(DWORD*)(obj + 68) & ~0xFFFFFFBF)
		{
			// If this is a manually scope object, remove it instantly
			NetConnection__detachObject(conn, ref);
		}
		else
		{
			// Otherwise just remove the scope flag like normal
			*(BYTE*)(ref + 11) &= 0xFEu;
		}

		return;
	}
}

void NetObject__clearScopeAlways(SimObject* obj, int argc, const char* argv[])
{
	DWORD object = (DWORD)obj;

	if (!(*(DWORD*)(object + 68) & 2))
	{
		*(DWORD*)(object + 68) &= 0xFFFFFFBF;

		// Remove from GhostAlwaysSet
		// Cannot figure out how to do in c++
		char buffer[128];
		sprintf_s(buffer, "GhostAlwaysSet.remove(%d);", *(DWORD*)(object + 32));
		Eval(buffer);

		// Remove from all scoped connections
		for (DWORD ref = *(DWORD*)(object + 76); ref; ref = *(DWORD*)(object + 76))
			NetConnection__detachObject(*(DWORD*)(ref + 28), ref);
	}
}

void NetObject__setNetFlag(SimObject* obj, int argc, const char* argv[])
{
	DWORD object = (DWORD)obj;

	// Crazy conversion of ts -> c++ boolean
	if (!_stricmp(argv[3], "true") || !_stricmp(argv[3], "1") || (0 != atoi(argv[3])))
		*(DWORD*)(object + 68) |= 1 << atoi(argv[2]);
	else
		*(DWORD*)(object + 68) &= ~(1 << atoi(argv[2]));
}

bool init()
{
	if (!torque_init())
		return false;

	// Address r2000-0001: 0x005CCDA0, r1986 0x005CCD50
	// NetConnection__detachObject = (NetConnection__detachObjectFn)(0x005CCDA0);
	NetConnection__detachObject = (NetConnection__detachObjectFn)ScanFunc("\x56\x8B\x74\x24\x08\x8B\x46\x04\x80\x4E\x0A\x20\x85\xC0\x57\x8B\xF9\x75\x0D\x56\xC7\x46\x04\xFF\xFF\xFF\xFF\xE8\x00\x00\x00\x00\x8B\x0E\x85\xC9\x74\x73\x8B\x46\x18\x85\xC0\x74\x08\x8B\x4E\x14\x89\x48\x14\xEB\x06\x8B\x56\x14\x89\x51\x4C\x8B\x46\x14\x85\xC0\x74\x06\x8B\x4E\x18\x89\x48\x18\xF6\x87\xB8\x00\x00\x00\x03\x75\x34\x8B\x16\xA1\x00\x00\x00\x00\x8B\x4A\x20\x8B\x97\x0C\x02\x00\x00\x48\x23\xC8\x8D\x04\x8A\x83\x38\x00\x74\x19\x8D\x64\x24\x00\x8B\x08\x3B\xCE\x74\x0A\x8D\x41\x20\x83\x38\x00\x75\xF2\xEB\x05\x8B\x49\x20\x89\x08\xC7\x46\x14\x00\x00\x00\x00\xC7\x46\x18\x00\x00\x00\x00\xC7\x06\x00\x00\x00\x00\x5F", "xxxxxxxxxxxxxxxxxxxxxxxxxxxx????xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx????xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
	
	// Address r2000-0001: 0x005CCE40, r1986 0x005CCE40
	// Tried finding a signature.. address does not seem to change anyway
	NetConnection__clearLocalScopeAlways = (NetConnection__clearLocalScopeAlwaysFn)0x005CCE40;

	// Create detour to hook
	Detour__clearLocalScopeAlways = new MologieDetours::Detour<NetConnection__clearLocalScopeAlwaysFn>(
		NetConnection__clearLocalScopeAlways, (NetConnection__clearLocalScopeAlwaysFn)Hooked__clearLocalScopeAlways);

	// Register console functions
	ConsoleFunction("NetObject", "clearScopeAlways", NetObject__clearScopeAlways, "NetObject::clearScopeAlways()", 2, 2);
	ConsoleFunction("NetObject", "setNetFlag", NetObject__setNetFlag, "NetObject::setNetFlag(int bit, bool flag)", 4, 4);

	// Output to client of how they can use them
	Printf("SelectiveGhosting | DLL Loaded");
	Printf("   NetObject::clearScopeAlways() - Removes from GhostAlwaysSet");
	Printf("   NetObject::setNetFlag(int bit, bool flag) - Sets how the object is sent to clients");
	return true;
}

int __stdcall DllMain(HINSTANCE hInstance, unsigned long reason, void* reserved)
{
	switch (reason)
	{
	case DLL_PROCESS_ATTACH:
		return init();

	case DLL_PROCESS_DETACH:
		if (Detour__clearLocalScopeAlways != NULL)
		{
			// Delete the detour so this does not crash the game if they reload this
			delete Detour__clearLocalScopeAlways;
		}

		return true;

	default:
		return true;
	}
}