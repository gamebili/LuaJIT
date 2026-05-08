/*
** These lauxlib entries are macros in official Lua 5.4 headers. A module can
** call them, but it must not be able to take old LuaJIT function addresses.
*/

#include "lua.h"
#include "lauxlib.h"

#if defined(LUA54_REJECT_PREPBUFFER)
char *(*lua54_reject_prepbuffer)(luaL_Buffer *) = luaL_prepbuffer;
#elif defined(LUA54_REJECT_ADDCHAR)
void (*lua54_reject_addchar)(luaL_Buffer *, int) = luaL_addchar;
#elif defined(LUA54_REJECT_ADDSIZE)
void (*lua54_reject_addsize)(luaL_Buffer *, size_t) = luaL_addsize;
#elif defined(LUA54_REJECT_BUFFADDR)
char *(*lua54_reject_buffaddr)(luaL_Buffer *) = luaL_buffaddr;
#elif defined(LUA54_REJECT_BUFFLEN)
size_t (*lua54_reject_bufflen)(luaL_Buffer *) = luaL_bufflen;
#elif defined(LUA54_REJECT_BUFFSUB)
void (*lua54_reject_buffsub)(luaL_Buffer *, size_t) = luaL_buffsub;
#elif defined(LUA54_REJECT_ARGCHECK)
void (*lua54_reject_argcheck)(lua_State *, int, int, const char *) =
  luaL_argcheck;
#elif defined(LUA54_REJECT_ARGEXPECTED)
void (*lua54_reject_argexpected)(lua_State *, int, int, const char *) =
  luaL_argexpected;
#elif defined(LUA54_REJECT_PUSHFAIL)
void (*lua54_reject_pushfail)(lua_State *) = luaL_pushfail;
#elif defined(LUA54_REJECT_CHECKSTRING)
const char *(*lua54_reject_checkstring)(lua_State *, int) = luaL_checkstring;
#elif defined(LUA54_REJECT_OPTSTRING)
const char *(*lua54_reject_optstring)(lua_State *, int, const char *) =
  luaL_optstring;
#elif defined(LUA54_REJECT_TYPENAME)
const char *(*lua54_reject_typename)(lua_State *, int) = luaL_typename;
#elif defined(LUA54_REJECT_LOADFILE)
int (*lua54_reject_loadfile)(lua_State *, const char *) = luaL_loadfile;
#elif defined(LUA54_REJECT_LOADBUFFER)
int (*lua54_reject_loadbuffer)(lua_State *, const char *, size_t,
			       const char *) = luaL_loadbuffer;
#elif defined(LUA54_REJECT_DOFILE)
int (*lua54_reject_dofile)(lua_State *, const char *) = luaL_dofile;
#elif defined(LUA54_REJECT_DOSTRING)
int (*lua54_reject_dostring)(lua_State *, const char *) = luaL_dostring;
#elif defined(LUA54_REJECT_GETMETATABLE)
int (*lua54_reject_getmetatable)(lua_State *, const char *) =
  luaL_getmetatable;
#elif defined(LUA54_REJECT_OPT)
lua_Integer (*lua54_reject_opt)(lua_State *, lua_Integer (*)(lua_State *, int),
				int, lua_Integer) = luaL_opt;
#elif defined(LUA54_REJECT_CHECKVERSION)
void (*lua54_reject_checkversion)(lua_State *) = luaL_checkversion;
#elif defined(LUA54_REJECT_INTOP)
lua_Integer (*lua54_reject_intop)(lua_Integer, lua_Integer) = luaL_intop;
#elif defined(LUA54_REJECT_NEWLIBTABLE)
void (*lua54_reject_newlibtable)(lua_State *, const luaL_Reg *) =
  luaL_newlibtable;
#elif defined(LUA54_REJECT_NEWLIB)
void (*lua54_reject_newlib)(lua_State *, const luaL_Reg *) = luaL_newlib;
#elif defined(LUA54_REJECT_WRITESTRING)
int (*lua54_reject_writestring)(const char *, size_t) = lua_writestring;
#elif defined(LUA54_REJECT_WRITELINE)
int (*lua54_reject_writeline)(void) = lua_writeline;
#elif defined(LUA54_REJECT_WRITESTRINGERROR)
int (*lua54_reject_writestringerror)(const char *, const char *) =
  lua_writestringerror;
#elif defined(LUA54_REJECT_ASSERT)
void (*lua54_reject_assert)(int) = lua_assert;
#else
#error "select a lauxlib macro-only symbol to reject"
#endif
