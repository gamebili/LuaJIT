/*
** Library initialization.
** Copyright (C) 2005-2026 Mike Pall. See Copyright Notice in luajit.h
**
** Major parts taken verbatim from the Lua interpreter.
** Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h
*/

#define lib_init_c
#define LUA_LIB

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include "lj_arch.h"

#if LJ_54
int luaopen_base_luajit(lua_State *L);
#define LJ_OPEN_BASE	luaopen_base_luajit
#else
#define LJ_OPEN_BASE	luaopen_base
#endif

static const luaL_Reg lj_lib_load[] = {
  { "",			LJ_OPEN_BASE },
  { LUA_LOADLIBNAME,	luaopen_package },
  { LUA_TABLIBNAME,	luaopen_table },
  { LUA_IOLIBNAME,	luaopen_io },
  { LUA_OSLIBNAME,	luaopen_os },
  { LUA_STRLIBNAME,	luaopen_string },
  { LUA_MATHLIBNAME,	luaopen_math },
  { LUA_DBLIBNAME,	luaopen_debug },
#if LJ_54
  /* utf8 is a Lua 5.4 standard library; keep it out of default LuaJIT mode. */
  { LUA_UTF8LIBNAME,	luaopen_utf8 },
#endif
#if !LJ_54
  { LUA_BITLIBNAME,	luaopen_bit },
#endif
  { LUA_JITLIBNAME,	luaopen_jit },
  { NULL,		NULL }
};

static const luaL_Reg lj_lib_preload[] = {
#if LJ_54
  /* Lua 5.4 mode hides the legacy global bit library at startup, but LuaJIT's
  ** own JIT tooling still uses require("bit"). Keep it available as an
  ** explicit extension module without restoring the initial global.
  */
  { LUA_BITLIBNAME,	luaopen_bit },
#endif
#if LJ_HASFFI
  { LUA_FFILIBNAME,	luaopen_ffi },
#endif
  { NULL,		NULL }
};

LUALIB_API void luaL_openlibs(lua_State *L)
{
  const luaL_Reg *lib;
  for (lib = lj_lib_load; lib->func; lib++) {
    lua_pushcfunction(L, lib->func);
    lua_pushstring(L, lib->name);
    lua_call(L, 1, 0);
  }
  luaL_findtable(L, LUA_REGISTRYINDEX, "_PRELOAD",
		 sizeof(lj_lib_preload)/sizeof(lj_lib_preload[0])-1);
  for (lib = lj_lib_preload; lib->func; lib++) {
    lua_pushcfunction(L, lib->func);
    lua_setfield(L, -2, lib->name);
  }
  lua_pop(L, 1);
}

