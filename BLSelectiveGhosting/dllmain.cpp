#include "torque.hpp"
#include "detours\detours.h"

BLFUNC(void, __thiscall, NetConnection__detachObject, DWORD conn, DWORD obj)
BLFUNC(void, __thiscall, NetConnection__clearLocalScopeAlways, DWORD conn, DWORD obj)
MologieDetours::Detour<NetConnection__clearLocalScopeAlwaysFn>* Detour__clearLocalScopeAlways;

void __fastcall Hooked__clearLocalScopeAlways(DWORD conn, void* blank, DWORD obj)
{
	if (!*(DWORD*)(conn + 492))
		return;

	DWORD hash = 4 * (*(DWORD*)(obj + 32) & (*(DWORD*)0x0071FB6C - 1));

	for (DWORD ref = *(DWORD*)(*(DWORD*)(conn + 524) + hash); ref; ref = *(DWORD*)(ref + 32))
	{
		if (*(DWORD*)ref != obj)
			continue;

		if (*(DWORD*)(obj + 68) & ~0xFFFFFFBF)
		{
			//If this is a manually scope object, remove it instantly
			NetConnection__detachObject(conn, ref);
		}
		else
		{
			//Otherwise just remove the scope flag like normal
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

		//Remove from GhostAlwaysSet
		//Cannot figure out how to do in c++
		char buffer[128];
		sprintf_s(buffer, "GhostAlwaysSet.remove(%d);", *(DWORD*)(object + 32));
		Eval(buffer);

		//Remove from all scoped connections
		for (DWORD ref = *(DWORD*)(object + 76); ref; ref = *(DWORD*)(object + 76))
			NetConnection__detachObject(*(DWORD*)(ref + 28), ref);
	}
}

void NetObject__setNetFlag(SimObject* obj, int argc, const char* argv[])
{
	DWORD object = (DWORD)obj;

	//Crazy conversion of ts -> c++ boolean
	if (!_stricmp(argv[3], "true") || !_stricmp(argv[3], "1") || (0 != atoi(argv[3])))
		*(DWORD*)(object + 68) |= 1 << atoi(argv[2]);
	else
		*(DWORD*)(object + 68) &= ~(1 << atoi(argv[2]));
}

bool init()
{
	if (!torque_init())
		return false;

	NetConnection__detachObject = (NetConnection__detachObjectFn)(0x005CCDA0); //r2000 0x005CCDA0, old r1986 0x005CCD50
	NetConnection__clearLocalScopeAlways = (NetConnection__clearLocalScopeAlwaysFn)(0x005CCE40); //r2000 0x005CCE40, old 1986 0x005CCDF0

	Detour__clearLocalScopeAlways = new MologieDetours::Detour<NetConnection__clearLocalScopeAlwaysFn>(
		NetConnection__clearLocalScopeAlways, (NetConnection__clearLocalScopeAlwaysFn)Hooked__clearLocalScopeAlways);

	ConsoleFunction("NetObject", "clearScopeAlways", NetObject__clearScopeAlways, "NetObject::clearScopeAlways()", 2, 2);
	ConsoleFunction("NetObject", "setNetFlag", NetObject__setNetFlag, "NetObject::setNetFlag(int bit, bool flag)", 4, 4);

	Printf("SelectiveGhosting | DLL Loaded");
	return true;
}

int __stdcall DllMain(HINSTANCE hInstance, unsigned long reason, void* reserved)
{
	switch (reason)
	{
	case DLL_PROCESS_ATTACH:
		return init();

	case DLL_PROCESS_DETACH:
		return true;

	default:
		return true;
	}
}