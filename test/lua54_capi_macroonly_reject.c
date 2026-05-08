/*
** Official Lua 5.4 exposes these convenience APIs as macros. External modules
** can call them, but they must not be able to take legacy LuaJIT function
** addresses through the Lua 5.4 compatibility headers.
*/

#include "lua.h"

#if defined(LUA54_REJECT_GETEXTRASPACE)
void *(*lua54_reject_getextraspace)(lua_State *) = lua_getextraspace;
#elif defined(LUA54_REJECT_UPVALUEINDEX)
int (*lua54_reject_upvalueindex)(int) = lua_upvalueindex;
#elif defined(LUA54_REJECT_TONUMBER)
lua_Number (*lua54_reject_tonumber)(lua_State *, int) = lua_tonumber;
#elif defined(LUA54_REJECT_TOINTEGER)
lua_Integer (*lua54_reject_tointeger)(lua_State *, int) = lua_tointeger;
#elif defined(LUA54_REJECT_NUMBERTOINTEGER)
int (*lua54_reject_numbertointeger)(lua_Number, lua_Integer *) =
  lua_numbertointeger;
#elif defined(LUA54_REJECT_PUSHGLOBALTABLE)
void (*lua54_reject_pushglobaltable)(lua_State *) = lua_pushglobaltable;
#elif defined(LUA54_REJECT_INSERT)
void (*lua54_reject_insert)(lua_State *, int) = lua_insert;
#elif defined(LUA54_REJECT_REMOVE)
void (*lua54_reject_remove)(lua_State *, int) = lua_remove;
#elif defined(LUA54_REJECT_REPLACE)
void (*lua54_reject_replace)(lua_State *, int) = lua_replace;
#elif defined(LUA54_REJECT_NEWUSERDATA)
void *(*lua54_reject_newuserdata)(lua_State *, size_t) = lua_newuserdata;
#elif defined(LUA54_REJECT_GETUSERVALUE)
int (*lua54_reject_getuservalue)(lua_State *, int) = lua_getuservalue;
#elif defined(LUA54_REJECT_SETUSERVALUE)
int (*lua54_reject_setuservalue)(lua_State *, int) = lua_setuservalue;
#elif defined(LUA54_REJECT_POP)
void (*lua54_reject_pop)(lua_State *, int) = lua_pop;
#elif defined(LUA54_REJECT_NEWTABLE)
void (*lua54_reject_newtable)(lua_State *) = lua_newtable;
#elif defined(LUA54_REJECT_REGISTER)
void (*lua54_reject_register)(lua_State *, const char *, lua_CFunction) =
  lua_register;
#elif defined(LUA54_REJECT_PUSHCFUNCTION)
void (*lua54_reject_pushcfunction)(lua_State *, lua_CFunction) =
  lua_pushcfunction;
#elif defined(LUA54_REJECT_PUSHLITERAL)
const char *(*lua54_reject_pushliteral)(lua_State *, const char *) =
  lua_pushliteral;
#elif defined(LUA54_REJECT_TOSTRING)
const char *(*lua54_reject_tostring)(lua_State *, int) = lua_tostring;
#elif defined(LUA54_REJECT_ISFUNCTION)
int (*lua54_reject_isfunction)(lua_State *, int) = lua_isfunction;
#elif defined(LUA54_REJECT_ISTABLE)
int (*lua54_reject_istable)(lua_State *, int) = lua_istable;
#elif defined(LUA54_REJECT_ISLIGHTUSERDATA)
int (*lua54_reject_islightuserdata)(lua_State *, int) = lua_islightuserdata;
#elif defined(LUA54_REJECT_ISNIL)
int (*lua54_reject_isnil)(lua_State *, int) = lua_isnil;
#elif defined(LUA54_REJECT_ISBOOLEAN)
int (*lua54_reject_isboolean)(lua_State *, int) = lua_isboolean;
#elif defined(LUA54_REJECT_ISTHREAD)
int (*lua54_reject_isthread)(lua_State *, int) = lua_isthread;
#elif defined(LUA54_REJECT_ISNONE)
int (*lua54_reject_isnone)(lua_State *, int) = lua_isnone;
#elif defined(LUA54_REJECT_ISNONEORNIL)
int (*lua54_reject_isnoneornil)(lua_State *, int) = lua_isnoneornil;
#else
#error "select a Lua 5.4 macro-only symbol to reject"
#endif
