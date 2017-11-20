#include <kernel/Process.hpp>
#include <kernel/Kernel.hpp>
#include <LuaBridge/LuaBridge.h>
#include <cppfs/FileHandle.h>
#include <cppfs/fs.h>
#include <iostream>

using namespace luabridge;

const string Process::LuaEntryPoint = "main.lua";
const string Process::AssetsEntryPoint = "assets";

Process::Process(const FilePath& executable,
				 vector<string> environment,
				 const uint64_t pid,
				 const uint64_t cartStart):
	environment(environment),
	pid(pid),
	mapped(false) {
	// Pontos de entrada no sistema de arquivos para c�digo
	// e dados do cart
	FilePath lua(executable);
	lua = lua.resolve(LuaEntryPoint);
	FilePath assets(executable);
	assets = assets.resolve(AssetsEntryPoint);
	
	// Carrega os assets para o cartridge (que ser� copiado para RAM
	// na localiza��o cartStart)
	cartridgeMemory = new CartridgeMemory(assets, cartStart);

	// Carrega o c�digo
	st = luaL_newstate();
	luaL_openlibs(st);
	luaL_dofile(st, (const char*)lua.toNative().c_str());

	cout << "pid " << pid << " loading cart " << lua.toNative() << endl;
}

Process::~Process() {
	lua_close(st);
}

void Process::addSyscalls() {
	getGlobalNamespace(st)
		.beginNamespace("kernel")
		.addFunction("write", &kernel_api_write)
		.addFunction("read", &kernel_api_read)
		.endNamespace();
}

void Process::update() {
	lua_getglobal(st, "update");
	if (lua_isfunction(st, -1)) {
		if (lua_pcall(st, 0, 0, 0) != 0) {
			cout << "pid " << pid << " update(): " << lua_tostring(st, -1) << endl;
			KernelSingleton->exit(pid);
		}
	}
	else {
		cout << "pid " << pid << " update() is not defined. exiting." << endl;
		KernelSingleton->exit(pid);
	}
}

void Process::draw() {
	lua_getglobal(st, "draw");
	if (lua_isfunction(st, -1)) {
		if (lua_pcall(st, 0, 0, 0) != 0) {
			cout << "pid " << pid << " draw(): " << lua_tostring(st, -1) << endl;
			KernelSingleton->exit(pid);
		}
	}
	else {
		cout << "pid " << pid << " draw() is not defined. exiting." << endl;
		KernelSingleton->exit(pid);
	}
}

const uint64_t Process::getPid() {
	return pid;
}

Memory* Process::getMemory() {
	mapped = true;
	return cartridgeMemory;
}

void Process::unmap() {
	mapped = false;
}

bool Process::isMapped() {
	return mapped;
}