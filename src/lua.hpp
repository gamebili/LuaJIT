// C++ wrapper for LuaJIT header files.

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#if !defined(LUAJIT_ENABLE_LUA54COMPAT) || defined(LUAJIT_INTERNAL_USE)
#include "luajit.h"
#endif
}

