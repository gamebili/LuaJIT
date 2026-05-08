/*
** Standard library header.
** Copyright (C) 2005-2026 Mike Pall. See Copyright Notice in luajit.h
*/

#if !defined(_LUALIB_H) && !defined(lualib_h)
#define _LUALIB_H
#ifdef LUAJIT_ENABLE_LUA54COMPAT
#define lualib_h
#endif

#include "lua.h"

/* lua.h hides this private selector after its own declarations; recompute it
** here so lualib.h can choose between official 5.4 and LuaJIT-only names.
*/
#undef LUAJIT_EXTERNAL_LUA54
#if defined(LUAJIT_ENABLE_LUA54COMPAT) && !defined(LUA_CORE) && \
    !defined(LUA_LIB) && !defined(LUAJIT_INTERNAL_USE)
#define LUAJIT_EXTERNAL_LUA54 1
#else
#define LUAJIT_EXTERNAL_LUA54 0
#endif

#ifdef LUAJIT_ENABLE_LUA54COMPAT
#define LUA_VERSUFFIX	"_" LUA_VERSION_MAJOR "_" LUA_VERSION_MINOR
#endif

#if !LUAJIT_EXTERNAL_LUA54
#ifndef LUA_GNAME
#define LUA_GNAME	"_G"
#endif
#ifndef LUA_FILEHANDLE
#define LUA_FILEHANDLE	"FILE*"
#endif
#endif

#define LUA_COLIBNAME	"coroutine"
#define LUA_MATHLIBNAME	"math"
#define LUA_STRLIBNAME	"string"
#define LUA_TABLIBNAME	"table"
#define LUA_IOLIBNAME	"io"
#define LUA_OSLIBNAME	"os"
#define LUA_LOADLIBNAME	"package"
#define LUA_DBLIBNAME	"debug"
#define LUA_UTF8LIBNAME	"utf8"
#if !LUAJIT_EXTERNAL_LUA54
#define LUA_BITLIBNAME	"bit"
#define LUA_JITLIBNAME	"jit"
#define LUA_FFILIBNAME	"ffi"
#endif

LUAMOD_API int luaopen_base(lua_State *L);
LUAMOD_API int luaopen_coroutine(lua_State *L);
LUAMOD_API int luaopen_math(lua_State *L);
LUAMOD_API int luaopen_string(lua_State *L);
LUAMOD_API int luaopen_table(lua_State *L);
LUAMOD_API int luaopen_io(lua_State *L);
LUAMOD_API int luaopen_os(lua_State *L);
LUAMOD_API int luaopen_package(lua_State *L);
LUAMOD_API int luaopen_debug(lua_State *L);
LUAMOD_API int luaopen_utf8(lua_State *L);
#if !LUAJIT_EXTERNAL_LUA54
LUALIB_API int luaopen_bit(lua_State *L);
LUALIB_API int luaopen_jit(lua_State *L);
LUALIB_API int luaopen_ffi(lua_State *L);
LUALIB_API int luaopen_string_buffer(lua_State *L);
#endif

LUALIB_API void luaL_openlibs(lua_State *L);

#if !LUAJIT_EXTERNAL_LUA54
#ifndef lua_assert
#define lua_assert(x)	((void)0)
#endif
#endif

#undef LUAJIT_EXTERNAL_LUA54

#endif
