/*
** These lauxlib entries are macros in official Lua 5.4 headers. A module can
** call them, but it must not be able to take old LuaJIT function addresses.
*/

#include "lua.h"
#include "lauxlib.h"

#if defined(LUA54_REJECT_PREPBUFFER)
char *(*lua54_reject_prepbuffer)(luaL_Buffer *) = luaL_prepbuffer;
#elif defined(LUA54_REJECT_ARGEXPECTED)
void (*lua54_reject_argexpected)(lua_State *, int, int, const char *) =
  luaL_argexpected;
#elif defined(LUA54_REJECT_PUSHFAIL)
void (*lua54_reject_pushfail)(lua_State *) = luaL_pushfail;
#elif defined(LUA54_REJECT_LOADFILE)
int (*lua54_reject_loadfile)(lua_State *, const char *) = luaL_loadfile;
#elif defined(LUA54_REJECT_LOADBUFFER)
int (*lua54_reject_loadbuffer)(lua_State *, const char *, size_t,
			       const char *) = luaL_loadbuffer;
#else
#error "select a lauxlib macro-only symbol to reject"
#endif
