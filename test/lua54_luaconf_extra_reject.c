#include "lua.h"

/*
** External Lua 5.4 modules should not see LuaJIT/Lua 5.1 private luaconf
** knobs. LuaJIT internals get these through LUAJIT_INTERNAL_USE instead.
*/

#ifdef LUA_PATH_CONFIG
#error LUA_PATH_CONFIG leaked from external Lua 5.4 luaconf.h
#endif

#ifdef LUA_QL
#error LUA_QL leaked from external Lua 5.4 luaconf.h
#endif

#ifdef LUA_QS
#error LUA_QS leaked from external Lua 5.4 luaconf.h
#endif

#ifdef LUA_MAXCAPTURES
#error LUA_MAXCAPTURES leaked from external Lua 5.4 luaconf.h
#endif

#ifdef LUAI_MAXCSTACK
#error LUAI_MAXCSTACK leaked from external Lua 5.4 luaconf.h
#endif

#ifdef LUAI_GCPAUSE
#error LUAI_GCPAUSE leaked from external Lua 5.4 luaconf.h
#endif

#ifdef LUAI_GCMUL
#error LUAI_GCMUL leaked from external Lua 5.4 luaconf.h
#endif

#ifdef LUA_NUMBER_DOUBLE
#error LUA_NUMBER_DOUBLE leaked from external Lua 5.4 luaconf.h
#endif

#ifdef LUA_NUMBER_SCAN
#error LUA_NUMBER_SCAN leaked from external Lua 5.4 luaconf.h
#endif

#ifdef LUAI_MAXNUMBER2STR
#error LUAI_MAXNUMBER2STR leaked from external Lua 5.4 luaconf.h
#endif

#ifdef LUA_INTFRMLEN
#error LUA_INTFRMLEN leaked from external Lua 5.4 luaconf.h
#endif

#ifdef LUA_INTFRM_T
#error LUA_INTFRM_T leaked from external Lua 5.4 luaconf.h
#endif

#ifdef luai_apicheck
#error luai_apicheck leaked from external Lua 5.4 luaconf.h
#endif

int main(void)
{
  return LUA_VERSION_NUM == 504 ? 0 : 1;
}
