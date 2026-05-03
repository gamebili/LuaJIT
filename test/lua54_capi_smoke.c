/*
** Smoke tests for the Lua 5.4 compatibility C API surface.
*/

#include <string.h>
#include <stdio.h>

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

static int require_open_count = 0;
static char warning_buf[64];
static int warning_tocont = -1;

static void check(lua_State *L, int cond, const char *msg)
{
  if (!cond) {
    fprintf(stderr, "%s\n", msg);
    luaL_error(L, "lua54 C API smoke failed: %s", msg);
  }
}

static void check_string(lua_State *L, int idx, const char *want,
			 const char *msg)
{
  const char *got = lua_tostring(L, idx);
  check(L, got != NULL && strcmp(got, want) == 0, msg);
}

static void check_integer(lua_State *L, int idx, lua_Integer want,
			  const char *msg)
{
  int ok = 0;
  lua_Integer got = lua_tointegerx(L, idx, &ok);
  check(L, ok && got == want, msg);
}

static int len_meta(lua_State *L)
{
  (void)L;
  lua_pushinteger(L, 77);
  return 1;
}

static int require_open(lua_State *L)
{
  check_string(L, 1, "capi.mod", "luaL_requiref passes module name");
  require_open_count++;
  lua_newtable(L);
  lua_pushliteral(L, "ready");
  lua_setfield(L, -2, "state");
  return 1;
}

static int push_answer(lua_State *L)
{
  lua_pushinteger(L, 42);
  return 1;
}

static void capture_warning(void *ud, const char *msg, int tocont)
{
  (void)ud;
  strncpy(warning_buf, msg, sizeof(warning_buf)-1);
  warning_buf[sizeof(warning_buf)-1] = '\0';
  warning_tocont = tocont;
}

static void test_stack_and_number_api(lua_State *L)
{
  lua_Integer iv = 0;
  void **extra;
  lua_State *co;

  lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_MAINTHREAD);
  check(L, lua_tothread(L, -1) == L, "LUA_RIDX_MAINTHREAD");
  lua_pop(L, 1);

  lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);
  check(L, lua_istable(L, -1), "LUA_RIDX_GLOBALS");
  lua_pop(L, 1);

  extra = (void **)lua_getextraspace(L);
  *extra = L;
  co = lua_newthread(L);
  check(L, *(void **)lua_getextraspace(co) == L, "lua_getextraspace copy");
  lua_pop(L, 1);

  check(L, lua_stringtonumber(L, "123") == 4, "lua_stringtonumber length");
  check_integer(L, -1, 123, "lua_stringtonumber value");
  lua_pop(L, 1);
  check(L, lua_stringtonumber(L, "nope") == 0, "lua_stringtonumber reject");

  check(L, lua_numbertointeger((lua_Number)42, &iv) && iv == 42,
	"lua_numbertointeger integer");
  check(L, !lua_numbertointeger((lua_Number)1.5, &iv),
	"lua_numbertointeger fraction");

  lua_pushliteral(L, "a");
  lua_pushliteral(L, "b");
  lua_pushliteral(L, "c");
  lua_pushliteral(L, "d");
  lua_rotate(L, -4, 1);
  check_string(L, -4, "d", "lua_rotate first");
  check_string(L, -3, "a", "lua_rotate second");
  check_string(L, -2, "b", "lua_rotate third");
  check_string(L, -1, "c", "lua_rotate fourth");
  lua_pop(L, 4);

  lua_pushcfunction(L, push_answer);
  lua_callk(L, 0, 1, 0, NULL);
  check_integer(L, -1, 42, "lua_callk macro");
  lua_pop(L, 1);

  lua_pushcfunction(L, push_answer);
  check(L, lua_pcallk(L, 0, 1, 0, 0, NULL) == LUA_OK, "lua_pcallk macro");
  check_integer(L, -1, 42, "lua_pcallk result");
  lua_pop(L, 1);
}

static void test_compare_len_arith(lua_State *L)
{
  lua_pushinteger(L, 2);
  lua_pushinteger(L, 3);
  check(L, lua_compare(L, -2, -1, LUA_OPLT), "lua_compare lt");
  check(L, lua_compare(L, -2, -1, LUA_OPLE), "lua_compare le");
  check(L, !lua_compare(L, -2, -1, LUA_OPEQ), "lua_compare eq false");
  lua_pop(L, 2);

  lua_newtable(L);
  lua_pushliteral(L, "x");
  lua_seti(L, -2, 1);
  lua_pushliteral(L, "y");
  lua_seti(L, -2, 2);
  lua_len(L, -1);
  check_integer(L, -1, 2, "lua_len table raw length");
  lua_pop(L, 2);

  lua_newtable(L);
  lua_newtable(L);
  lua_pushcfunction(L, len_meta);
  lua_setfield(L, -2, "__len");
  lua_setmetatable(L, -2);
  lua_len(L, -1);
  check_integer(L, -1, 77, "lua_len __len metamethod");
  lua_pop(L, 2);

  lua_pushinteger(L, 5);
  lua_pushinteger(L, 2);
  lua_arith(L, LUA_OPADD);
  check_integer(L, -1, 7, "lua_arith add");
  lua_pop(L, 1);

  lua_pushinteger(L, 7);
  lua_pushinteger(L, 3);
  lua_arith(L, LUA_OPIDIV);
  check_integer(L, -1, 2, "lua_arith idiv");
  lua_pop(L, 1);

  lua_pushinteger(L, 6);
  lua_pushinteger(L, 3);
  lua_arith(L, LUA_OPBAND);
  check_integer(L, -1, 2, "lua_arith band");
  lua_pop(L, 1);
}

static void test_uservalue_api(lua_State *L)
{
  int top;
  void *ud = lua_newuserdatauv(L, 4, 2);
  check(L, ud != NULL, "lua_newuserdatauv");
  lua_pushliteral(L, "uv1");
  check(L, lua_setiuservalue(L, -2, 1) == 1, "lua_setiuservalue #1");
  check(L, lua_getiuservalue(L, -1, 1) == LUA_TSTRING,
	"lua_getiuservalue #1 type");
  check_string(L, -1, "uv1", "lua_getiuservalue #1 value");
  lua_pop(L, 1);
  check(L, lua_getiuservalue(L, -1, 2) == LUA_TNIL,
	"lua_getiuservalue declared nil");
  lua_pop(L, 1);
  top = lua_gettop(L);
  check(L, lua_getiuservalue(L, -1, 3) == LUA_TNONE,
	"lua_getiuservalue out of range");
  check(L, lua_gettop(L) == top, "lua_getiuservalue none pushes nothing");
  lua_pushnil(L);
  check(L, lua_setiuservalue(L, -2, 3) == 0,
	"lua_setiuservalue out of range");
  check(L, lua_gettop(L) == top, "lua_setiuservalue invalid pops value");
  lua_pop(L, 1);
}

static void test_lauxlib_api(lua_State *L)
{
  luaL_Buffer b;
  char *p;

  luaL_checkversion(L);
  luaL_argexpected(L, 1, 1, "truthy condition");

  luaL_pushfail(L);
  check(L, lua_isnil(L, -1), "luaL_pushfail pushes nil");
  lua_pop(L, 1);

  lua_newtable(L);
  check(L, luaL_getsubtable(L, -1, "child") == 0,
	"luaL_getsubtable creates");
  check(L, lua_istable(L, -1), "luaL_getsubtable created table");
  lua_pop(L, 1);
  check(L, luaL_getsubtable(L, -1, "child") == 1,
	"luaL_getsubtable reuses");
  lua_pop(L, 2);

  check(L, luaL_getsubtable(L, LUA_REGISTRYINDEX, LUA_LOADED_TABLE) == 1,
	"LUA_LOADED_TABLE");
  lua_pop(L, 1);

  luaL_requiref(L, "capi.mod", require_open, 1);
  check(L, require_open_count == 1, "luaL_requiref calls opener once");
  lua_getfield(L, -1, "state");
  check_string(L, -1, "ready", "luaL_requiref module result");
  lua_pop(L, 2);
  luaL_requiref(L, "capi.mod", require_open, 1);
  check(L, require_open_count == 1, "luaL_requiref reuses loaded module");
  lua_pop(L, 1);

  lua_pushboolean(L, 1);
  luaL_tolstring(L, -1, NULL);
  check_string(L, -1, "true", "luaL_tolstring boolean");
  lua_pop(L, 2);

  luaL_buffinit(L, &b);
  p = luaL_prepbuffer(&b);
  check(L, p == luaL_buffaddr(&b), "luaL_buffaddr");
  memcpy(p, "abcd", 4);
  luaL_addsize(&b, 4);
  check(L, luaL_bufflen(&b) == 4, "luaL_bufflen");
  luaL_buffsub(&b, 1);
  luaL_pushresult(&b);
  check_string(L, -1, "abc", "luaL_buffsub result");
  lua_pop(L, 1);

  luaL_buffinit(L, &b);
  p = luaL_prepbuffer(&b);
  memcpy(p, "xy", 2);
  luaL_pushresultsize(&b, 2);
  check_string(L, -1, "xy", "luaL_pushresultsize");
  lua_pop(L, 1);
}

static void test_warning_and_gc_api(lua_State *L)
{
  int oldmode;
  lua_Debug ar;

  lua_setwarnf(L, capture_warning, NULL);
  lua_warning(L, "captured", 0);
  check(L, strcmp(warning_buf, "captured") == 0 && warning_tocont == 0,
	"lua_warning callback");

  oldmode = lua_gc(L, LUA_GCGEN, 0);
  check(L, oldmode == LUA_GCGEN || oldmode == LUA_GCINC, "LUA_GCGEN");
  oldmode = lua_gc(L, LUA_GCINC, 0);
  check(L, oldmode == LUA_GCGEN, "LUA_GCINC previous mode");
  oldmode = lua_gc(L, LUA_GCGEN, 0);
  check(L, oldmode == LUA_GCINC, "LUA_GCGEN previous mode");

  memset(&ar, 0, sizeof(ar));
  lua_pushcfunction(L, push_answer);
  check(L, lua_getinfo(L, ">ut", &ar), "lua_getinfo >ut");
  check(L, ar.nparams == 0 && ar.isvararg == 1, "lua_Debug u fields");
  check(L, ar.istailcall == 0 && ar.ftransfer == 0 && ar.ntransfer == 0,
	"lua_Debug t/transfer fields");
}

int main(void)
{
  lua_State *L = luaL_newstate();
  if (L == NULL)
    return 2;
  luaL_openlibs(L);
  test_stack_and_number_api(L);
  test_compare_len_arith(L);
  test_uservalue_api(L);
  test_lauxlib_api(L);
  test_warning_and_gc_api(L);
  lua_close(L);
  return 0;
}
