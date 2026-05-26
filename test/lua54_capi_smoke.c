/*
** Smoke tests for the Lua 5.4 compatibility C API surface.
*/

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <stddef.h>
#include <limits.h>
#include <stdarg.h>
#include <math.h>

#include "lua.h"
#include "lauxlib.h"

#define CAPI_STATIC_ASSERT(name, cond) \
  typedef char capi_static_assert_##name[(cond) ? 1 : -1]

CAPI_STATIC_ASSERT(lua_debug_srclen_after_source,
  offsetof(lua_Debug, srclen) == offsetof(lua_Debug, source) + sizeof(const char *));
CAPI_STATIC_ASSERT(lua_debug_nups_is_uchar,
  sizeof(((lua_Debug *)0)->nups) == sizeof(unsigned char));
CAPI_STATIC_ASSERT(lua_debug_nparams_is_uchar,
  sizeof(((lua_Debug *)0)->nparams) == sizeof(unsigned char));
CAPI_STATIC_ASSERT(lua_debug_isvararg_is_char,
  sizeof(((lua_Debug *)0)->isvararg) == sizeof(char));
CAPI_STATIC_ASSERT(lua_debug_istailcall_is_char,
  sizeof(((lua_Debug *)0)->istailcall) == sizeof(char));
CAPI_STATIC_ASSERT(lua_debug_transfer_before_short_src,
  offsetof(lua_Debug, ftransfer) < offsetof(lua_Debug, short_src));
CAPI_STATIC_ASSERT(lua_debug_private_ci_is_pointer_sized,
  sizeof(((lua_Debug *)0)->i_ci) == sizeof(void *));
#ifdef LUA_UNSIGNED
CAPI_STATIC_ASSERT(lua_unsigned_matches_public_type,
  sizeof(LUA_UNSIGNED) == sizeof(lua_Unsigned));
CAPI_STATIC_ASSERT(lua_unsigned_matches_integer_width,
  sizeof(lua_Unsigned) == sizeof(lua_Integer));
CAPI_STATIC_ASSERT(lua_maxinteger_matches_public_width,
  sizeof(lua_Integer) <= sizeof(int) ||
  LUA_MAXINTEGER > (lua_Integer)0x7fffffffu);
CAPI_STATIC_ASSERT(lua_mininteger_matches_public_width,
  sizeof(lua_Integer) <= sizeof(int) ||
  LUA_MININTEGER < (lua_Integer)(-2147483647 - 1));
#endif

#ifdef LUA_GLOBALSINDEX
#error "Lua 5.4 compatibility header must not expose LUA_GLOBALSINDEX"
#endif

#ifdef LUA_ENVIRONINDEX
#error "Lua 5.4 compatibility header must not expose LUA_ENVIRONINDEX"
#endif

#ifdef lua_strlen
#error "Lua 5.4 compatibility header must not expose lua_strlen"
#endif

#ifndef LUA_VERSION_MAJOR
#error "Lua 5.4 compatibility header must expose LUA_VERSION_MAJOR"
#endif

#ifndef LUA_VERSION_MINOR
#error "Lua 5.4 compatibility header must expose LUA_VERSION_MINOR"
#endif

#ifndef LUA_VERSION_RELEASE
#error "Lua 5.4 compatibility header must expose LUA_VERSION_RELEASE"
#endif

#ifndef LUA_VERSION_RELEASE_NUM
#error "Lua 5.4 compatibility header must expose LUA_VERSION_RELEASE_NUM"
#endif

#ifndef LUA_NUMTYPES
#error "Lua 5.4 compatibility header must expose LUA_NUMTYPES"
#endif

#ifndef LUA_NUMTAGS
#error "Lua 5.4 compatibility header must expose LUA_NUMTAGS"
#endif

#ifndef LUA_RIDX_LAST
#error "Lua 5.4 compatibility header must expose LUA_RIDX_LAST"
#endif

#ifndef LUA_EXTRASPACE
#error "Lua 5.4 compatibility header must expose LUA_EXTRASPACE"
#endif

#ifndef LUAI_MAXALIGN
#error "Lua 5.4 compatibility header must expose LUAI_MAXALIGN"
#endif

#ifndef LUA_NUMBER_FRMLEN
#error "Lua 5.4 compatibility header must expose LUA_NUMBER_FRMLEN"
#endif

#ifndef LUA_INTEGER_FRMLEN
#error "Lua 5.4 compatibility header must expose LUA_INTEGER_FRMLEN"
#endif

#ifndef LUA_INTEGER_FMT
#error "Lua 5.4 compatibility header must expose LUA_INTEGER_FMT"
#endif

#ifndef LUAI_UACINT
#error "Lua 5.4 compatibility header must expose LUAI_UACINT"
#endif

#ifndef LUA_UNSIGNED
#error "Lua 5.4 compatibility header must expose LUA_UNSIGNED"
#endif

#ifndef LUA_MAXUNSIGNED
#error "Lua 5.4 compatibility header must expose LUA_MAXUNSIGNED"
#endif

#ifndef LUA_GNAME
#error "Lua 5.4 lauxlib header must expose LUA_GNAME"
#endif

#ifndef LUA_FILEHANDLE
#error "Lua 5.4 lauxlib header must expose LUA_FILEHANDLE"
#endif

#ifndef LUA_LOADED_TABLE
#error "Lua 5.4 lauxlib header must expose LUA_LOADED_TABLE"
#endif

#ifndef LUA_PRELOAD_TABLE
#error "Lua 5.4 lauxlib header must expose LUA_PRELOAD_TABLE"
#endif

#ifndef lua_assert
#error "Lua 5.4 lauxlib header must expose lua_assert"
#endif

typedef char lua54_buffer_init_field[
  sizeof(((luaL_Buffer *)0)->init.b) == LUAL_BUFFERSIZE ? 1 : -1
];

#ifndef lua_writestring
#error "Lua 5.4 lauxlib header must expose lua_writestring"
#endif

#ifndef lua_writeline
#error "Lua 5.4 lauxlib header must expose lua_writeline"
#endif

#ifndef lua_writestringerror
#error "Lua 5.4 lauxlib header must expose lua_writestringerror"
#endif

#ifndef luaL_prepbuffer
#error "Lua 5.4 lauxlib header must expose luaL_prepbuffer macro"
#endif

#ifndef luaL_argexpected
#error "Lua 5.4 lauxlib header must expose luaL_argexpected macro"
#endif

#ifndef luaL_pushfail
#error "Lua 5.4 lauxlib header must expose luaL_pushfail macro"
#endif

#ifndef luaL_loadfile
#error "Lua 5.4 lauxlib header must expose luaL_loadfile macro"
#endif

#ifndef luaL_loadbuffer
#error "Lua 5.4 lauxlib header must expose luaL_loadbuffer macro"
#endif

#ifndef luaL_argcheck
#error "Lua 5.4 lauxlib header must expose luaL_argcheck macro"
#endif

#ifndef luaL_checkstring
#error "Lua 5.4 lauxlib header must expose luaL_checkstring macro"
#endif

#ifndef luaL_optstring
#error "Lua 5.4 lauxlib header must expose luaL_optstring macro"
#endif

#ifndef luaL_typename
#error "Lua 5.4 lauxlib header must expose luaL_typename macro"
#endif

#ifndef luaL_dofile
#error "Lua 5.4 lauxlib header must expose luaL_dofile macro"
#endif

#ifndef luaL_dostring
#error "Lua 5.4 lauxlib header must expose luaL_dostring macro"
#endif

#ifndef luaL_getmetatable
#error "Lua 5.4 lauxlib header must expose luaL_getmetatable macro"
#endif

#ifndef luaL_opt
#error "Lua 5.4 lauxlib header must expose luaL_opt macro"
#endif

#ifndef luaL_newlibtable
#error "Lua 5.4 lauxlib header must expose luaL_newlibtable macro"
#endif

#ifndef luaL_newlib
#error "Lua 5.4 lauxlib header must expose luaL_newlib macro"
#endif

#ifndef lua_newuserdata
#error "Lua 5.4 compatibility header must expose lua_newuserdata alias macro"
#endif

#ifndef lua_getuservalue
#error "Lua 5.4 compatibility header must expose lua_getuservalue alias macro"
#endif

#ifndef lua_setuservalue
#error "Lua 5.4 compatibility header must expose lua_setuservalue alias macro"
#endif

#ifndef lua_insert
#error "Lua 5.4 compatibility header must expose lua_insert macro"
#endif

#ifndef lua_remove
#error "Lua 5.4 compatibility header must expose lua_remove macro"
#endif

#ifndef lua_replace
#error "Lua 5.4 compatibility header must expose lua_replace macro"
#endif

#ifndef lua_tonumber
#error "Lua 5.4 compatibility header must expose lua_tonumber macro"
#endif

#ifndef lua_tointeger
#error "Lua 5.4 compatibility header must expose lua_tointeger macro"
#endif

#ifndef lua_getextraspace
#error "Lua 5.4 compatibility header must expose lua_getextraspace macro"
#endif

#ifndef lua_call
#error "Lua 5.4 compatibility header must expose lua_call macro"
#endif

#ifndef lua_pcall
#error "Lua 5.4 compatibility header must expose lua_pcall macro"
#endif

#ifndef lua_yield
#error "Lua 5.4 compatibility header must expose lua_yield macro"
#endif

#include "lualib.h"

#ifndef LUA_VERSUFFIX
#error "Lua 5.4 lualib header must expose LUA_VERSUFFIX"
#endif

#ifndef LUAMOD_API
#error "Lua 5.4 compatibility header must expose LUAMOD_API"
#endif

static int require_open_count = 0;
static char warning_buf[64];
static int warning_tocont = -1;
static int close_call_count = 0;
static int close_nil_error_count = 0;
static int close_body_error_count = 0;
static int hook_call_ftransfer = -1;
static int hook_call_ntransfer = -1;
static int hook_ret_ftransfer = -1;
static int hook_ret_ntransfer = -1;
static int hook_vararg_call_ftransfer = -1;
static int hook_vararg_call_ntransfer = -1;
static int hook_vararg_ret_ftransfer = -1;
static int hook_vararg_ret_ntransfer = -1;
static int hook_c_call_ftransfer = -1;
static int hook_c_call_ntransfer = -1;
static int hook_c_ret_ftransfer = -1;
static int hook_c_ret_ntransfer = -1;
static int hook_tail_ftransfer = -1;
static int hook_tail_ntransfer = -1;
static int hook_tail_istailcall = -1;
static int hook_tail_vararg_call_ftransfer = -1;
static int hook_tail_vararg_call_ntransfer = -1;
static int hook_tail_vararg_ret_ftransfer = -1;
static int hook_tail_vararg_ret_ntransfer = -1;
static int hook_tail_vararg_ret_istailcall = -1;

static int record_close(lua_State *L);
static void push_closeable(lua_State *L, lua_CFunction closef);

static void header_output_macros_compile_only(void)
{
  lua_writestring("", 0);
  lua_writeline();
  lua_writestringerror("%s", "");
}

typedef struct CApiReaderCtx {
  const char *src;
  size_t len;
} CApiReaderCtx;

typedef struct DumpBuffer {
  char data[65536];
  size_t len;
} DumpBuffer;

typedef struct AllocCtx {
  int calls;
  int frees;
} AllocCtx;

typedef struct TrackingAllocCtx {
  size_t live;
  size_t peak;
  int calls;
} TrackingAllocCtx;

typedef struct SwitchAllocCtx {
  int calls;
  int allocs;
  int reallocs;
  int frees;
} SwitchAllocCtx;

typedef struct ShrinkFailAllocCtx {
  int calls;
  int frees;
  int fail_shrink;
  int shrink_fails;
  size_t max_failed_shrink_osize;
} ShrinkFailAllocCtx;

typedef struct FakeHugeAllocCtx {
  lua_Alloc oldf;
  void *oldud;
  size_t fake_threshold;
  size_t fake_size;
  int fake_allocs;
  int fake_frees;
  int fake_reallocs;
  char fake_block;
} FakeHugeAllocCtx;

typedef struct StrictAllocBlock {
  void *ptr;
  size_t size;
} StrictAllocBlock;

typedef struct StrictAllocCtx {
  StrictAllocBlock *blocks;
  int capacity;
  int live_blocks;
  int calls;
  int frees;
  int fail_shrink;
  int fail_grow;
  int shrink_fails;
  int grow_fails;
  int bad_osize;
  int missing_ptr;
  int alloc_requests;
  int fail_at_alloc;
  int fail_once_alloc;
  int call_fails;
  size_t fail_shrink_min_osize;
  size_t fail_grow_min_nsize;
  size_t max_failed_shrink_osize;
  size_t max_failed_grow_nsize;
} StrictAllocCtx;

typedef int (*RawGetI54Sig)(lua_State *L, int idx, lua_Integer n);
typedef void (*RawSetI54Sig)(lua_State *L, int idx, lua_Integer n);
typedef int (*LuaOpenBaseSig)(lua_State *L);
typedef int (*LuaOpenCoroutineSig)(lua_State *L);
typedef lua_Number (*LuaVersionValueSig)(lua_State *L);
typedef void (*LuaCallKSig)(lua_State *L, int nargs, int nresults,
			    lua_KContext ctx, lua_KFunction k);
typedef int (*LuaPCallKSig)(lua_State *L, int nargs, int nresults,
			    int errfunc, lua_KContext ctx, lua_KFunction k);
typedef int (*LuaYieldKSig)(lua_State *L, int nresults, lua_KContext ctx,
			    lua_KFunction k);
typedef lua_Unsigned (*LuaRawLenSig)(lua_State *L, int idx);
typedef const char *(*LuaPushLStringSig)(lua_State *L, const char *s,
					 size_t len);
typedef const char *(*LuaPushStringSig)(lua_State *L, const char *s);
typedef int (*LuaGetTableSig)(lua_State *L, int idx);
typedef int (*LuaGetFieldSig)(lua_State *L, int idx, const char *k);
typedef int (*LuaGetISig)(lua_State *L, int idx, lua_Integer n);
typedef int (*LuaRawGetSig)(lua_State *L, int idx);
typedef int (*LuaRawGetISig)(lua_State *L, int idx, lua_Integer n);
typedef int (*LuaRawGetPSig)(lua_State *L, int idx, const void *p);
typedef void (*LuaRawSetISig)(lua_State *L, int idx, lua_Integer n);
typedef int (*LuaGetGlobalSig)(lua_State *L, const char *name);
typedef void (*LuaSetGlobalSig)(lua_State *L, const char *name);
typedef int (*LuaLoadSig)(lua_State *L, lua_Reader reader, void *data,
			  const char *chunkname, const char *mode);
typedef int (*LuaDumpSig)(lua_State *L, lua_Writer writer, void *data,
			  int strip);
typedef int (*LuaResumeSig)(lua_State *L, lua_State *from, int nargs,
			    int *nresults);

static LuaOpenBaseSig luaopen_base_sig = luaopen_base;
static LuaOpenCoroutineSig luaopen_coroutine_sig = luaopen_coroutine;
static LuaVersionValueSig lua_version_value_sig = lua_version;
static LuaCallKSig lua_callk_sig = lua_callk;
static LuaPCallKSig lua_pcallk_sig = lua_pcallk;
static LuaYieldKSig lua_yieldk_sig = lua_yieldk;
static LuaRawLenSig lua_rawlen_sig = lua_rawlen;
static LuaPushLStringSig lua_pushlstring_sig = lua_pushlstring;
static LuaPushStringSig lua_pushstring_sig = lua_pushstring;
static LuaGetTableSig lua_gettable_sig = lua_gettable;
static LuaGetFieldSig lua_getfield_sig = lua_getfield;
static LuaGetISig lua_geti_sig = lua_geti;
static LuaRawGetSig lua_rawget_sig = lua_rawget;
static LuaRawGetISig lua_rawgeti_sig = lua_rawgeti;
static LuaRawGetPSig lua_rawgetp_sig = lua_rawgetp;
static LuaRawSetISig lua_rawseti_sig = lua_rawseti;
static LuaGetGlobalSig lua_getglobal_sig = lua_getglobal;
static LuaSetGlobalSig lua_setglobal_sig = lua_setglobal;
static LuaLoadSig lua_load_sig = lua_load;
static LuaDumpSig lua_dump_sig = lua_dump;
static LuaResumeSig lua_resume_sig = lua_resume;

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

static int panic_a(lua_State *L)
{
  (void)L;
  return 0;
}

static int panic_b(lua_State *L)
{
  (void)L;
  return 0;
}

static void capi_transfer_hook(lua_State *L, lua_Debug *ar)
{
  if (!lua_getinfo(L, "nrutS", ar))
    return;
  if (ar->nparams == 2 && !ar->isvararg) {
    if (ar->event == LUA_HOOKCALL) {
      hook_call_ftransfer = ar->ftransfer;
      hook_call_ntransfer = ar->ntransfer;
    } else if (ar->event == LUA_HOOKTAILCALL) {
      hook_tail_ftransfer = ar->ftransfer;
      hook_tail_ntransfer = ar->ntransfer;
      hook_tail_istailcall = ar->istailcall;
    } else if (ar->event == LUA_HOOKRET) {
      hook_ret_ftransfer = ar->ftransfer;
      hook_ret_ntransfer = ar->ntransfer;
    }
  } else if (ar->nparams == 1 && ar->isvararg) {
    /* Lua 5.4 reports only the fixed parameter as call input for varargs. */
    if (ar->event == LUA_HOOKCALL) {
      hook_vararg_call_ftransfer = ar->ftransfer;
      hook_vararg_call_ntransfer = ar->ntransfer;
    } else if (ar->event == LUA_HOOKRET) {
      hook_vararg_ret_ftransfer = ar->ftransfer;
      hook_vararg_ret_ntransfer = ar->ntransfer;
    }
  } else if (ar->nparams == 0 && ar->isvararg && ar->istailcall) {
    if (ar->event == LUA_HOOKTAILCALL) {
      hook_tail_vararg_call_ftransfer = ar->ftransfer;
      hook_tail_vararg_call_ntransfer = ar->ntransfer;
    } else if (ar->event == LUA_HOOKRET) {
      hook_tail_vararg_ret_ftransfer = ar->ftransfer;
      hook_tail_vararg_ret_ntransfer = ar->ntransfer;
      hook_tail_vararg_ret_istailcall = ar->istailcall;
    }
  } else if (ar->event == LUA_HOOKCALL && ar->what != NULL &&
	     strcmp(ar->what, "C") == 0 && hook_c_call_ftransfer < 0) {
    hook_c_call_ftransfer = ar->ftransfer;
    hook_c_call_ntransfer = ar->ntransfer;
  } else if (ar->event == LUA_HOOKRET && ar->what != NULL &&
	     strcmp(ar->what, "C") == 0 && hook_c_ret_ftransfer < 0) {
    hook_c_ret_ftransfer = ar->ftransfer;
    hook_c_ret_ntransfer = ar->ntransfer;
  }
}

static void *counting_alloc(void *ud, void *ptr, size_t osize, size_t nsize)
{
  AllocCtx *ctx = (AllocCtx *)ud;
  (void)osize;
  ctx->calls++;
  if (nsize == 0) {
    ctx->frees++;
    free(ptr);
    return NULL;
  }
  return realloc(ptr, nsize);
}

static void *tracking_alloc(void *ud, void *ptr, size_t osize, size_t nsize)
{
  TrackingAllocCtx *ctx = (TrackingAllocCtx *)ud;
  void *np;
  ctx->calls++;
  if (ptr == NULL)
    osize = 0;
  if (nsize == 0) {
    if (ctx->live >= osize)
      ctx->live -= osize;
    free(ptr);
    return NULL;
  }
  np = realloc(ptr, nsize);
  if (np != NULL) {
    if (nsize >= osize)
      ctx->live += nsize - osize;
    else if (ctx->live >= osize - nsize)
      ctx->live -= osize - nsize;
    if (ctx->live > ctx->peak)
      ctx->peak = ctx->live;
  }
  return np;
}

static void *switching_alloc(void *ud, void *ptr, size_t osize, size_t nsize)
{
  SwitchAllocCtx *ctx = (SwitchAllocCtx *)ud;
  (void)osize;
  ctx->calls++;
  if (nsize == 0) {
    ctx->frees++;
    free(ptr);
    return NULL;
  }
  if (ptr == NULL)
    ctx->allocs++;
  else
    ctx->reallocs++;
  return realloc(ptr, nsize);
}

static void *shrink_fail_alloc(void *ud, void *ptr, size_t osize, size_t nsize)
{
  ShrinkFailAllocCtx *ctx = (ShrinkFailAllocCtx *)ud;
  ctx->calls++;
  if (nsize == 0) {
    ctx->frees++;
    free(ptr);
    return NULL;
  }
  if (ctx->fail_shrink && ptr != NULL && nsize < osize) {
    ctx->shrink_fails++;
    if (osize > ctx->max_failed_shrink_osize)
      ctx->max_failed_shrink_osize = osize;
    return NULL;
  }
  return realloc(ptr, nsize);
}

static void *fake_huge_alloc(void *ud, void *ptr, size_t osize, size_t nsize)
{
  FakeHugeAllocCtx *ctx = (FakeHugeAllocCtx *)ud;
  void *fake = (void *)&ctx->fake_block;
  if (ptr == fake) {
    if (nsize == 0) {
      ctx->fake_frees++;
      return NULL;
    }
    ctx->fake_reallocs++;
    ctx->fake_size = nsize;
    return fake;
  }
  if (ptr == NULL && nsize >= ctx->fake_threshold) {
    ctx->fake_allocs++;
    ctx->fake_size = nsize;
    (void)osize;
    return fake;
  }
  return ctx->oldf(ctx->oldud, ptr, osize, nsize);
}

static int strict_alloc_find(StrictAllocCtx *ctx, void *ptr)
{
  int i;
  for (i = 0; i < ctx->live_blocks; i++)
    if (ctx->blocks[i].ptr == ptr)
      return i;
  return -1;
}

static void strict_alloc_add(StrictAllocCtx *ctx, void *ptr, size_t size)
{
  if (ctx->live_blocks < ctx->capacity) {
    ctx->blocks[ctx->live_blocks].ptr = ptr;
    ctx->blocks[ctx->live_blocks].size = size;
    ctx->live_blocks++;
  } else {
    ctx->missing_ptr++;
  }
}

static void strict_alloc_remove(StrictAllocCtx *ctx, int idx)
{
  ctx->live_blocks--;
  ctx->blocks[idx] = ctx->blocks[ctx->live_blocks];
}

static int strict_alloc_has_block_at_least(StrictAllocCtx *ctx, size_t size)
{
  int i;
  for (i = 0; i < ctx->live_blocks; i++)
    if (ctx->blocks[i].size >= size)
      return 1;
  return 0;
}

static int strict_alloc_should_fail(StrictAllocCtx *ctx)
{
  if (ctx->fail_at_alloc > 0 &&
      ++ctx->alloc_requests >= ctx->fail_at_alloc) {
    ctx->call_fails++;
    if (ctx->fail_once_alloc) {
      ctx->fail_at_alloc = 0;
      ctx->fail_once_alloc = 0;
    }
    return 1;
  }
  return 0;
}

static void *strict_alloc(void *ud, void *ptr, size_t osize, size_t nsize)
{
  StrictAllocCtx *ctx = (StrictAllocCtx *)ud;
  void *np;
  int idx;
  ctx->calls++;
  if (ptr == NULL) {
    if (nsize == 0)
      return NULL;
    if (osize != 0)
      ctx->bad_osize++;
    if (strict_alloc_should_fail(ctx))
      return NULL;
    np = malloc(nsize);
    if (np != NULL)
      strict_alloc_add(ctx, np, nsize);
    return np;
  }
  idx = strict_alloc_find(ctx, ptr);
  if (idx < 0) {
    ctx->missing_ptr++;
  } else if (ctx->blocks[idx].size != osize) {
    ctx->bad_osize++;
  }
  if (nsize == 0) {
    ctx->frees++;
    free(ptr);
    if (idx >= 0)
      strict_alloc_remove(ctx, idx);
    return NULL;
  }
  if (strict_alloc_should_fail(ctx))
    return NULL;
  if (ctx->fail_shrink && nsize < osize &&
      osize >= ctx->fail_shrink_min_osize) {
    ctx->shrink_fails++;
    if (osize > ctx->max_failed_shrink_osize)
      ctx->max_failed_shrink_osize = osize;
    return NULL;
  }
  if (ctx->fail_grow && nsize > osize &&
      nsize >= ctx->fail_grow_min_nsize) {
    ctx->grow_fails++;
    if (nsize > ctx->max_failed_grow_nsize)
      ctx->max_failed_grow_nsize = nsize;
    return NULL;
  }
  np = realloc(ptr, nsize);
  if (np != NULL && idx >= 0) {
    ctx->blocks[idx].ptr = np;
    ctx->blocks[idx].size = nsize;
  }
  return np;
}

static int enable_shrink_fail_alloc(lua_State *L)
{
  void *ud = NULL;
  lua_getallocf(L, &ud);
  ((ShrinkFailAllocCtx *)ud)->fail_shrink = 1;
  return 0;
}

static int enable_strict_shrink_fail_alloc(lua_State *L)
{
  void *ud = NULL;
  lua_getallocf(L, &ud);
  ((StrictAllocCtx *)ud)->fail_shrink = 1;
  return 0;
}

static int strict_fail_once_after_alloc(lua_State *L)
{
  void *ud = NULL;
  StrictAllocCtx *ctx;
  int after = (int)luaL_checkinteger(L, 1);
  lua_getallocf(L, &ud);
  ctx = (StrictAllocCtx *)ud;
  ctx->fail_at_alloc = ctx->alloc_requests + after;
  ctx->fail_once_alloc = 1;
  return 0;
}

static int setallocf_null_function(lua_State *L)
{
  lua_setallocf(L, NULL, NULL);
  return 0;
}

static int warning_null_message(lua_State *L)
{
  lua_warning(L, NULL, 0);
  return 0;
}

static int raise_after_big_buffer(lua_State *L)
{
  luaL_Buffer b;
  size_t big = (size_t)LUAL_BUFFERSIZE * 32u;
  char *p;
  luaL_buffinit(L, &b);
  p = luaL_prepbuffsize(&b, big);
  memset(p, 'm', big);
  luaL_addsize(&b, big);
  /* This deliberately aborts before luaL_pushresult(). Lua 5.4 keeps the
  ** large buffer in a to-be-closed box so error unwinding releases it.
  */
  return luaL_error(L, "abort after luaL_Buffer growth");
}

static int return_big_buffer(lua_State *L)
{
  luaL_Buffer b;
  size_t big = (size_t)LUAL_BUFFERSIZE * 24u;
  char *p;
  luaL_buffinit(L, &b);
  p = luaL_prepbuffsize(&b, big);
  memset(p, 's', big);
  luaL_pushresultsize(&b, big);
  return 1;
}

static int return_big_addvalue_buffer(lua_State *L)
{
  luaL_Buffer src;
  luaL_Buffer b;
  size_t big = (size_t)LUAL_BUFFERSIZE * 18u;
  char *p;
  luaL_buffinit(L, &src);
  p = luaL_prepbuffsize(&src, big);
  memset(p, 'v', big);
  luaL_pushresultsize(&src, big);
  luaL_buffinit(L, &b);
  lua_insert(L, -2);
  luaL_addvalue(&b);
  luaL_pushresult(&b);
  return 1;
}

static int fail_growing_buffer(lua_State *L)
{
  luaL_Buffer b;
  size_t big = (size_t)LUAL_BUFFERSIZE * 16u;
  char *p;
  luaL_buffinit(L, &b);
  p = luaL_prepbuffsize(&b, big);
  memset(p, 'g', big);
  luaL_addsize(&b, big);
  p = luaL_prepbuffsize(&b, big);
  memset(p, 'u', big);
  luaL_pushresultsize(&b, big);
  return 1;
}

static int overflow_laux_buffer(lua_State *L)
{
  luaL_Buffer b;
  size_t huge = ~(size_t)0 - 63u;
  luaL_buffinit(L, &b);
  (void)luaL_prepbuffsize(&b, huge);
  luaL_addsize(&b, huge);
  (void)luaL_prepbuffsize(&b, 128u);
  return 0;
}

static void test_newstate_allocator_failure(lua_State *L)
{
  int limit;
  int saw_partial_failure = 0;
  for (limit = 1; limit <= 160; limit++) {
    StrictAllocCtx ctx;
    lua_State *T;
    memset(&ctx, 0, sizeof(ctx));
    ctx.capacity = 8192;
    ctx.fail_at_alloc = limit;
    ctx.blocks = (StrictAllocBlock *)calloc((size_t)ctx.capacity,
					    sizeof(StrictAllocBlock));
    check(L, ctx.blocks != NULL,
	  "lua_newstate failure allocator bookkeeping");
    T = lua_newstate(strict_alloc, &ctx);
    if (T != NULL) {
      ctx.fail_at_alloc = 0;
      lua_close(T);
    } else {
      check(L, ctx.call_fails > 0,
	    "lua_newstate failure must come from allocator");
      if (limit > 1 && ctx.frees > 0)
	saw_partial_failure = 1;
    }
    check(L, ctx.bad_osize == 0 && ctx.missing_ptr == 0,
	  "lua_newstate failure preserves allocator block sizes");
    check(L, ctx.live_blocks == 0,
	  "lua_newstate failure releases partial allocations");
    free(ctx.blocks);
  }
  check(L, saw_partial_failure,
	"lua_newstate partial initialization failure exercised");
}

static int fail_createtable_after_alloc(lua_State *L)
{
  void *ud = NULL;
  StrictAllocCtx *ctx;
  int after = (int)luaL_checkinteger(L, 1);
  lua_getallocf(L, &ud);
  ctx = (StrictAllocCtx *)ud;
  ctx->fail_at_alloc = ctx->alloc_requests + after;
  lua_createtable(L, 128, 128);
  ctx->fail_at_alloc = 0;
  return 1;
}

static void test_createtable_allocator_failure(lua_State *L, lua_State *T,
					       StrictAllocCtx *ctx)
{
  int limit;
  int saw_failure = 0;
  int saw_partial_cleanup = 0;
  int saw_success_after_failure = 0;
  lua_gc(T, LUA_GCCOLLECT, 0);
  for (limit = 1; limit <= 4; limit++) {
    int before_live = ctx->live_blocks;
    int before_fails = ctx->call_fails;
    int before_frees = ctx->frees;
    int failed = 0;
    lua_pushcfunction(T, fail_createtable_after_alloc);
    lua_pushinteger(T, limit);
    {
      int status = lua_pcall(T, 1, 1, 0);
      ctx->fail_at_alloc = 0;
      if (status == LUA_OK) {
	check(L, lua_istable(T, -1),
	      "lua_createtable allocator success result");
	lua_pushinteger(T, 54);
	lua_rawseti(T, -2, 128);
	lua_rawgeti(T, -1, 128);
	check(L, lua_tointeger(T, -1) == 54,
	      "lua_createtable allocator success table is usable");
	if (saw_failure)
	  saw_success_after_failure = 1;
      } else {
	check(L, status == LUA_ERRMEM,
	      "lua_createtable allocator failure reports memory error");
	check(L, ctx->call_fails > before_fails,
	      "lua_createtable failure must come from allocator");
	saw_failure = 1;
	failed = 1;
      }
      lua_settop(T, 0);
      lua_gc(T, LUA_GCCOLLECT, 0);
      lua_gc(T, LUA_GCCOLLECT, 0);
    }
    if (failed && ctx->frees > before_frees)
      saw_partial_cleanup = 1;
    check(L, ctx->bad_osize == 0 && ctx->missing_ptr == 0,
	  "lua_createtable failure preserves allocator block sizes");
    check(L, ctx->live_blocks == before_live,
	  "lua_createtable failure releases partial allocations");
  }
  check(L, saw_failure, "lua_createtable allocator failure exercised");
  check(L, saw_partial_cleanup,
	"lua_createtable partial allocation cleanup exercised");
  check(L, saw_success_after_failure,
	"lua_createtable failure scan reaches successful creation boundary");
}

static int fail_newmetatable_after_alloc(lua_State *L)
{
  void *ud = NULL;
  StrictAllocCtx *ctx;
  int after = (int)luaL_checkinteger(L, 1);
  int id = (int)luaL_checkinteger(L, 2);
  int created;
  char name[64];
  sprintf(name, "strict.alloc.mt.%d", id);
  lua_getallocf(L, &ud);
  ctx = (StrictAllocCtx *)ud;
  if (after > 0)
    ctx->fail_at_alloc = ctx->alloc_requests + after;
  created = luaL_newmetatable(L, name);
  ctx->fail_at_alloc = 0;
  lua_pushboolean(L, created);
  lua_insert(L, -2);
  return 2;
}

static void check_newmetatable_result(lua_State *L, lua_State *T,
				      int want_created, const char *name)
{
  const char *got;
  check(L, lua_toboolean(T, -2) == want_created,
	"luaL_newmetatable allocator created flag");
  check(L, lua_istable(T, -1), "luaL_newmetatable allocator result table");
  check(L, lua_getfield(T, -1, "__name") == LUA_TSTRING,
	"luaL_newmetatable allocator __name type");
  got = lua_tostring(T, -1);
  check(L, got != NULL && strcmp(got, name) == 0,
	"luaL_newmetatable allocator __name value");
  lua_pop(T, 1);
}

static void test_newmetatable_allocator_failure(lua_State *L)
{
  int limit;
  int saw_failure = 0;
  int saw_partial_cleanup = 0;
  int saw_success_after_failure = 0;
  for (limit = 1; limit <= 12; limit++) {
    StrictAllocCtx ctx;
    lua_State *T;
    int before_live;
    int before_fails;
    int before_frees;
    int status;
    int failed = 0;
    char name[64];
    sprintf(name, "strict.alloc.mt.%d", limit);
    memset(&ctx, 0, sizeof(ctx));
    ctx.capacity = 8192;
    ctx.blocks = (StrictAllocBlock *)calloc((size_t)ctx.capacity,
					    sizeof(StrictAllocBlock));
    check(L, ctx.blocks != NULL,
	  "luaL_newmetatable allocator failure bookkeeping");
    T = lua_newstate(strict_alloc, &ctx);
    check(L, T != NULL, "lua_newstate luaL_newmetatable allocator failure");
    lua_gc(T, LUA_GCCOLLECT, 0);
    before_live = ctx.live_blocks;
    before_fails = ctx.call_fails;
    before_frees = ctx.frees;
    lua_pushcfunction(T, fail_newmetatable_after_alloc);
    lua_pushinteger(T, limit);
    lua_pushinteger(T, limit);
    status = lua_pcall(T, 2, 2, 0);
    ctx.fail_at_alloc = 0;
    if (status == LUA_OK) {
      check_newmetatable_result(L, T, 1, name);
    } else {
      check(L, status == LUA_ERRMEM,
	    "luaL_newmetatable allocator failure reports memory error");
      check(L, ctx.call_fails > before_fails,
	    "luaL_newmetatable failure must come from allocator");
      saw_failure = 1;
      failed = 1;
      lua_settop(T, 0);
      lua_gc(T, LUA_GCCOLLECT, 0);
      lua_gc(T, LUA_GCCOLLECT, 0);
      check(L, ctx.live_blocks == before_live,
	    "luaL_newmetatable failure does not publish partial metatable");
      lua_getfield(T, LUA_REGISTRYINDEX, name);
      check(L, lua_isnil(T, -1),
	    "luaL_newmetatable failure leaves registry name unset");
      lua_pop(T, 1);
      lua_pushcfunction(T, fail_newmetatable_after_alloc);
      lua_pushinteger(T, 0);
      lua_pushinteger(T, limit);
      status = lua_pcall(T, 2, 2, 0);
      check(L, status == LUA_OK,
	    "luaL_newmetatable allocator retry status");
      check_newmetatable_result(L, T, 1, name);
      saw_success_after_failure = 1;
    }
    lua_settop(T, 0);
    lua_gc(T, LUA_GCCOLLECT, 0);
    lua_gc(T, LUA_GCCOLLECT, 0);
    if (failed && ctx.frees > before_frees)
      saw_partial_cleanup = 1;
    check(L, ctx.bad_osize == 0 && ctx.missing_ptr == 0,
	  "luaL_newmetatable failure preserves allocator block sizes");
    lua_close(T);
    check(L, ctx.live_blocks == 0,
	  "luaL_newmetatable failure releases state allocations");
    free(ctx.blocks);
  }
  check(L, saw_failure, "luaL_newmetatable allocator failure exercised");
  check(L, saw_partial_cleanup,
	"luaL_newmetatable partial allocation cleanup exercised");
  check(L, saw_success_after_failure,
	"luaL_newmetatable failure retry creates complete metatable");
}

static int fail_newthread_after_alloc(lua_State *L)
{
  void *ud = NULL;
  StrictAllocCtx *ctx;
  int after = (int)luaL_checkinteger(L, 1);
  lua_getallocf(L, &ud);
  ctx = (StrictAllocCtx *)ud;
  ctx->fail_at_alloc = ctx->alloc_requests + after;
  lua_newthread(L);
  ctx->fail_at_alloc = 0;
  return 1;
}

static void test_newthread_allocator_failure(lua_State *L, lua_State *T,
					     StrictAllocCtx *ctx)
{
  int limit;
  int saw_partial_failure = 0;
  lua_gc(T, LUA_GCCOLLECT, 0);
  for (limit = 1; limit <= 4; limit++) {
    int before_live = ctx->live_blocks;
    int before_fails = ctx->call_fails;
    lua_pushcfunction(T, fail_newthread_after_alloc);
    lua_pushinteger(T, limit);
    {
      int status = lua_pcall(T, 1, 1, 0);
      ctx->fail_at_alloc = 0;
      if (status == LUA_OK) {
	lua_pop(T, 1);
      } else {
	check(L, status == LUA_ERRMEM,
	      "lua_newthread allocator failure reports memory error");
	check(L, ctx->call_fails > before_fails,
	      "lua_newthread failure must come from allocator");
	lua_pop(T, 1);
	if (limit > 1)
	  saw_partial_failure = 1;
      }
    }
    lua_gc(T, LUA_GCCOLLECT, 0);
    check(L, ctx->bad_osize == 0 && ctx->missing_ptr == 0,
	  "lua_newthread failure preserves allocator block sizes");
    check(L, ctx->live_blocks == before_live,
	  "lua_newthread failure releases partial allocations");
  }
  check(L, saw_partial_failure,
	"lua_newthread partial initialization failure exercised");
}

static int push_allocator_upvalue(lua_State *L)
{
  lua_pushvalue(L, lua_upvalueindex(1));
  return 1;
}

static int fail_pushcclosure_after_alloc(lua_State *L)
{
  void *ud = NULL;
  StrictAllocCtx *ctx;
  int after = (int)luaL_checkinteger(L, 1);
  int nup = (int)luaL_checkinteger(L, 2);
  int i;
  lua_getallocf(L, &ud);
  ctx = (StrictAllocCtx *)ud;
  for (i = 0; i < nup; i++)
    lua_pushinteger(L, i + 1);
  ctx->fail_at_alloc = ctx->alloc_requests + after;
  lua_pushcclosure(L, push_allocator_upvalue, nup);
  ctx->fail_at_alloc = 0;
  return 1;
}

static void test_pushcclosure_allocator_failure(lua_State *L, lua_State *T,
					       StrictAllocCtx *ctx)
{
  int limit;
  int saw_failure = 0;
  int saw_success_after_failure = 0;
  lua_gc(T, LUA_GCCOLLECT, 0);
  for (limit = 1; limit <= 4; limit++) {
    int before_live = ctx->live_blocks;
    int before_fails = ctx->call_fails;
    lua_pushcfunction(T, fail_pushcclosure_after_alloc);
    lua_pushinteger(T, limit);
    lua_pushinteger(T, 8);
    {
      int status = lua_pcall(T, 2, 1, 0);
      ctx->fail_at_alloc = 0;
      if (status == LUA_OK) {
	int ok = 0;
	lua_Integer got;
	check(L, lua_isfunction(T, -1),
	      "lua_pushcclosure allocator success result");
	lua_call(T, 0, 1);
	got = lua_tointegerx(T, -1, &ok);
	check(L, ok && got == 1,
	      "lua_pushcclosure allocator success upvalue");
	if (saw_failure)
	  saw_success_after_failure = 1;
      } else {
	check(L, status == LUA_ERRMEM,
	      "lua_pushcclosure allocator failure reports memory error");
	check(L, ctx->call_fails > before_fails,
	      "lua_pushcclosure failure must come from allocator");
	saw_failure = 1;
      }
      lua_settop(T, 0);
      lua_gc(T, LUA_GCCOLLECT, 0);
    }
    check(L, ctx->bad_osize == 0 && ctx->missing_ptr == 0,
	  "lua_pushcclosure failure preserves allocator block sizes");
    check(L, ctx->live_blocks == before_live,
	  "lua_pushcclosure failure releases partial allocations");
  }
  check(L, saw_failure, "lua_pushcclosure allocator failure exercised");
  check(L, saw_success_after_failure,
	"lua_pushcclosure failure scan reaches successful closure boundary");
}

static int fail_newuserdatauv_after_alloc(lua_State *L)
{
  void *ud = NULL;
  StrictAllocCtx *ctx;
  int after = (int)luaL_checkinteger(L, 1);
  size_t size = (size_t)luaL_checkinteger(L, 2);
  int nuvalue = (int)luaL_checkinteger(L, 3);
  lua_getallocf(L, &ud);
  ctx = (StrictAllocCtx *)ud;
  ctx->fail_at_alloc = ctx->alloc_requests + after;
  (void)lua_newuserdatauv(L, size, nuvalue);
  ctx->fail_at_alloc = 0;
  return 1;
}

static void test_newuserdatauv_allocator_failure(lua_State *L, lua_State *T,
						 StrictAllocCtx *ctx)
{
  int limit;
  int saw_partial_cleanup = 0;
  lua_gc(T, LUA_GCCOLLECT, 0);
  for (limit = 1; limit <= 8; limit++) {
    int before_live = ctx->live_blocks;
    int before_fails = ctx->call_fails;
    int before_frees = ctx->frees;
    lua_pushcfunction(T, fail_newuserdatauv_after_alloc);
    lua_pushinteger(T, limit);
    lua_pushinteger(T, 256);
    lua_pushinteger(T, 4);
    {
      int status = lua_pcall(T, 3, 1, 0);
      ctx->fail_at_alloc = 0;
      if (status == LUA_OK) {
	check(L, lua_touserdata(T, -1) != NULL,
	      "lua_newuserdatauv allocator success result");
      } else {
	check(L, status == LUA_ERRMEM,
	      "lua_newuserdatauv allocator failure reports memory error");
	check(L, ctx->call_fails > before_fails,
	      "lua_newuserdatauv failure must come from allocator");
      }
      lua_settop(T, 0);
      lua_gc(T, LUA_GCCOLLECT, 0);
      if (status != LUA_OK && ctx->frees > before_frees)
	saw_partial_cleanup = 1;
    }
    check(L, ctx->bad_osize == 0 && ctx->missing_ptr == 0,
	  "lua_newuserdatauv failure preserves allocator block sizes");
    check(L, ctx->live_blocks == before_live,
	  "lua_newuserdatauv failure releases partial allocations");
  }
  check(L, saw_partial_cleanup,
	"lua_newuserdatauv partial allocation cleanup exercised");
}

#define LUA54_ALLOC_ARRAY16 \
  "1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,"
#define LUA54_ALLOC_ARRAY256 \
  LUA54_ALLOC_ARRAY16 LUA54_ALLOC_ARRAY16 \
  LUA54_ALLOC_ARRAY16 LUA54_ALLOC_ARRAY16 \
  LUA54_ALLOC_ARRAY16 LUA54_ALLOC_ARRAY16 \
  LUA54_ALLOC_ARRAY16 LUA54_ALLOC_ARRAY16 \
  LUA54_ALLOC_ARRAY16 LUA54_ALLOC_ARRAY16 \
  LUA54_ALLOC_ARRAY16 LUA54_ALLOC_ARRAY16 \
  LUA54_ALLOC_ARRAY16 LUA54_ALLOC_ARRAY16 \
  LUA54_ALLOC_ARRAY16 LUA54_ALLOC_ARRAY16

static const char lua54_parser_array_chunk[] =
  "local t = {" LUA54_ALLOC_ARRAY256 "}\n"
  "return t[1]\n";

static const char lua54_parser_exact_array_chunk[] =
  "local t = { marker = 17, " LUA54_ALLOC_ARRAY256 "}\n"
  "return t[1], t[256], t.marker\n";

static const char lua54_table_growth_chunk[] =
  "local after = ...\n"
  "local okjit, jitmod = pcall(require, 'jit')\n"
  "if okjit then jitmod.off() end\n"
  "strict_fail_once_after_alloc(after)\n"
  "local t = {}\n"
  "for i = 1, 128 do t[i] = i end\n"
  "for i = 1, 4096 do t[i + 0.5] = i end\n"
  "for i = 1, 128 do assert(t[i] == i) end\n"
  "return t[4096.5]\n";

static const char lua54_table_shrink_failure_chunk[] =
  "local after = ...\n"
  "local okjit, jitmod = pcall(require, 'jit')\n"
  "if okjit then jitmod.off() end\n"
  "local keys = {}\n"
  "for i = 1, 32 do keys[i] = {} end\n"
  "local t = {}\n"
  "for i = 1, 256 do t[i] = i end\n"
  "for i = 65, 256 do t[i] = nil end\n"
  "local function insert_all()\n"
  "  for i = 1, #keys do t[keys[i]] = i end\n"
  "end\n"
  "strict_fail_once_after_alloc(after)\n"
  "local ok = pcall(insert_all)\n"
  "local old_ok = t[1] == 1 and t[64] == 64 and "
  "t[65] == nil and t[256] == nil\n"
  "local new_seen = 0\n"
  "for i = 1, #keys do if t[keys[i]] ~= nil then new_seen = new_seen + 1 end end\n"
  "return ok, old_ok, new_seen\n";

static const char lua54_string_allocator_chunk[] =
  "local last = ''\n"
  "for i = 1, 1536 do\n"
  "  last = 'strict-string-allocator-' .. i .. '-' .. i\n"
  "end\n"
  "return #last\n";

static int fail_load_array_after_alloc(lua_State *L)
{
  void *ud = NULL;
  StrictAllocCtx *ctx;
  int after = (int)luaL_checkinteger(L, 1);
  int status;
  lua_getallocf(L, &ud);
  ctx = (StrictAllocCtx *)ud;
  ctx->fail_at_alloc = ctx->alloc_requests + after;
  status = luaL_loadbufferx(L, lua54_parser_array_chunk,
			    sizeof(lua54_parser_array_chunk) - 1u,
			    "=strict-parser-array", "t");
  ctx->fail_at_alloc = 0;
  if (status == LUA_OK || status == LUA_ERRMEM || status == LUA_ERRSYNTAX)
    lua_pop(L, 1);
  lua_pushinteger(L, status);
  return 1;
}

static int fail_load_exact_array_after_alloc(lua_State *L)
{
  void *ud = NULL;
  StrictAllocCtx *ctx;
  int after = (int)luaL_checkinteger(L, 1);
  int status;
  lua_getallocf(L, &ud);
  ctx = (StrictAllocCtx *)ud;
  ctx->fail_at_alloc = ctx->alloc_requests + after;
  status = luaL_loadbufferx(L, lua54_parser_exact_array_chunk,
			    sizeof(lua54_parser_exact_array_chunk) - 1u,
			    "=strict-parser-exact-array", "t");
  ctx->fail_at_alloc = 0;
  if (status == LUA_OK || status == LUA_ERRMEM || status == LUA_ERRSYNTAX)
    lua_pop(L, 1);
  lua_pushinteger(L, status);
  return 1;
}

static void test_parser_allocator_failure(lua_State *L, lua_State *T,
					  StrictAllocCtx *ctx)
{
  int limit;
  int saw_load_failure = 0;
  int saw_partial_cleanup = 0;
  int saw_success_after_failure = 0;
  lua_gc(T, LUA_GCCOLLECT, 0);
  for (limit = 1; limit <= 160; limit++) {
    int before_live = ctx->live_blocks;
    int before_fails = ctx->call_fails;
    int before_frees = ctx->frees;
    int failed = 0;
    lua_pushcfunction(T, fail_load_array_after_alloc);
    lua_pushinteger(T, limit);
    {
      int status = lua_pcall(T, 1, 1, 0);
      ctx->fail_at_alloc = 0;
      if (status == LUA_OK) {
	int ok = 0;
	int load_status = (int)lua_tointegerx(T, -1, &ok);
	check(L, ok, "parser allocator failure status result");
	if (load_status == LUA_OK) {
	  check(L, ctx->call_fails == before_fails,
		"parser allocator success must not hide allocator failure");
	  if (saw_load_failure)
	    saw_success_after_failure = 1;
	} else {
	  check(L, load_status == LUA_ERRMEM,
		"parser allocator failure reports memory error status");
	  check(L, ctx->call_fails > before_fails,
		"parser allocator status failure must come from allocator");
	  failed = 1;
	}
      } else {
	check(L, status == LUA_ERRMEM,
	      "parser allocator failure reports memory error");
	check(L, ctx->call_fails > before_fails,
	      "parser allocator failure must come from allocator");
	failed = 1;
      }
      lua_settop(T, 0);
      lua_gc(T, LUA_GCCOLLECT, 0);
      if (failed) {
	saw_load_failure = 1;
	if (ctx->frees > before_frees)
	  saw_partial_cleanup = 1;
      }
    }
    check(L, ctx->bad_osize == 0,
	  "parser allocator failure preserves allocator block sizes");
    check(L, ctx->missing_ptr == 0,
	  "parser allocator failure releases known allocator pointers");
    check(L, ctx->live_blocks == before_live,
	  "parser allocator failure releases partial allocations");
  }
  check(L, saw_load_failure, "parser allocator failure exercised");
  check(L, saw_partial_cleanup,
	"parser partial allocation cleanup exercised");
  check(L, saw_success_after_failure,
	"parser allocator failure scan reaches successful load boundary");
}

static void test_parser_exact_array_allocator_failure(lua_State *L,
						      lua_State *T,
						      StrictAllocCtx *ctx)
{
  int limit;
  int saw_load_failure = 0;
  int saw_partial_cleanup = 0;
  int saw_success_after_failure = 0;
  lua_gc(T, LUA_GCCOLLECT, 0);
  for (limit = 1; limit <= 220; limit++) {
    int before_live = ctx->live_blocks;
    int before_fails = ctx->call_fails;
    int before_frees = ctx->frees;
    int failed = 0;
    lua_pushcfunction(T, fail_load_exact_array_after_alloc);
    lua_pushinteger(T, limit);
    {
      int status = lua_pcall(T, 1, 1, 0);
      ctx->fail_at_alloc = 0;
      if (status == LUA_OK) {
	int ok = 0;
	int load_status = (int)lua_tointegerx(T, -1, &ok);
	check(L, ok, "parser exact-array allocator failure status result");
	if (load_status == LUA_OK) {
	  check(L, ctx->call_fails == before_fails,
		"parser exact-array success must not hide allocator failure");
	  if (saw_load_failure)
	    saw_success_after_failure = 1;
	} else {
	  check(L, load_status == LUA_ERRMEM,
		"parser exact-array failure reports memory error status");
	  check(L, ctx->call_fails > before_fails,
		"parser exact-array status failure must come from allocator");
	  failed = 1;
	}
      } else {
	check(L, status == LUA_ERRMEM,
	      "parser exact-array failure reports memory error");
	check(L, ctx->call_fails > before_fails,
	      "parser exact-array failure must come from allocator");
	failed = 1;
      }
      lua_settop(T, 0);
      lua_gc(T, LUA_GCCOLLECT, 0);
      if (failed) {
	saw_load_failure = 1;
	if (ctx->frees > before_frees)
	  saw_partial_cleanup = 1;
      }
    }
    check(L, ctx->bad_osize == 0,
	  "parser exact-array failure preserves allocator block sizes");
    check(L, ctx->missing_ptr == 0,
	  "parser exact-array failure releases known allocator pointers");
    check(L, ctx->live_blocks == before_live,
	  "parser exact-array failure releases partial allocations");
  }
  check(L, saw_load_failure,
	"parser exact-array allocator failure exercised");
  check(L, saw_partial_cleanup,
	"parser exact-array partial allocation cleanup exercised");
  check(L, saw_success_after_failure,
	"parser exact-array failure scan reaches successful load boundary");
}

static void test_string_allocator_failure(lua_State *L)
{
  int limit;
  int saw_string_failure = 0;
  int saw_nonfatal_resize_failure = 0;
  int saw_partial_cleanup = 0;
  for (limit = 1; limit <= 768; limit++) {
    StrictAllocCtx ctx;
    lua_State *T;
    int status;
    int before_fails;
    int before_frees;
    memset(&ctx, 0, sizeof(ctx));
    ctx.capacity = 8192;
    ctx.blocks = (StrictAllocBlock *)calloc((size_t)ctx.capacity,
					    sizeof(StrictAllocBlock));
    check(L, ctx.blocks != NULL,
	  "string allocator failure bookkeeping");
    T = lua_newstate(strict_alloc, &ctx);
    check(L, T != NULL, "lua_newstate string allocator failure");
    luaL_openlibs(T);
    status = luaL_dostring(T, "if jit then jit.off(); jit.flush() end");
    check(L, status == LUA_OK, "string allocator failure disables JIT");
    lua_settop(T, 0);
    status = luaL_loadbufferx(T, lua54_string_allocator_chunk,
			      sizeof(lua54_string_allocator_chunk) - 1u,
			      "=strict-string-alloc-fail", "t");
    check(L, status == LUA_OK, "string allocator failure probe load");
    before_fails = ctx.call_fails;
    before_frees = ctx.frees;
    ctx.fail_once_alloc = 1;
    ctx.fail_at_alloc = ctx.alloc_requests + limit;
    status = lua_pcall(T, 0, 1, 0);
    ctx.fail_at_alloc = 0;
    ctx.fail_once_alloc = 0;
    if (status == LUA_OK) {
      int ok = 0;
      lua_Integer got = lua_tointegerx(T, -1, &ok);
      check(L, ok && got > 0,
	    "string allocator failure success result");
      if (ctx.call_fails > before_fails)
	saw_nonfatal_resize_failure = 1;
    } else {
      check(L, status == LUA_ERRMEM,
	    "string allocator failure reports memory error");
      check(L, ctx.call_fails > before_fails,
	    "string allocator failure must come from allocator");
      saw_string_failure = 1;
      if (ctx.frees > before_frees)
	saw_partial_cleanup = 1;
    }
    lua_close(T);
    check(L, ctx.bad_osize == 0 && ctx.missing_ptr == 0,
	  "string allocator failure preserves block sizes");
    check(L, ctx.live_blocks == 0,
	  "string allocator failure releases state allocations");
    free(ctx.blocks);
  }
  check(L, saw_string_failure, "string allocator failure exercised");
  check(L, saw_nonfatal_resize_failure,
	"string allocator nonfatal resize failure exercised");
  check(L, saw_partial_cleanup,
	"string allocator failure cleanup exercised");
}

static void test_table_allocator_failure(lua_State *L, lua_State *T,
					 StrictAllocCtx *ctx)
{
  int limit;
  int saw_table_failure = 0;
  int saw_partial_cleanup = 0;
  int status;
  lua_gc(T, LUA_GCCOLLECT, 0);
  lua_settop(T, 0);
  status = luaL_loadbufferx(T, lua54_table_growth_chunk,
			    sizeof(lua54_table_growth_chunk) - 1u,
			    "=strict-table-growth", "t");
  check(L, status == LUA_OK, "table allocator failure probe load");
  for (limit = 1; limit <= 64; limit++) {
    int before_live;
    int before_fails;
    int before_frees;
    int failed = 0;
    lua_gc(T, LUA_GCCOLLECT, 0);
    before_live = ctx->live_blocks;
    before_fails = ctx->call_fails;
    before_frees = ctx->frees;
    lua_pushvalue(T, 1);
    lua_pushinteger(T, limit);
    status = lua_pcall(T, 1, 1, 0);
    ctx->fail_at_alloc = 0;
    ctx->fail_once_alloc = 0;
    if (status == LUA_OK) {
      int ok = 0;
      lua_Integer got = lua_tointegerx(T, -1, &ok);
      check(L, ok && got == 4096,
	    "table allocator failure success result");
      check(L, ctx->call_fails == before_fails,
	    "table allocator success must not hide allocator failure");
    } else {
      check(L, status == LUA_ERRMEM,
	    "table allocator failure reports memory error");
      check(L, ctx->call_fails > before_fails,
	    "table allocator failure must come from allocator");
      failed = 1;
      saw_table_failure = 1;
    }
    lua_settop(T, 1);
    lua_gc(T, LUA_GCCOLLECT, 0);
    lua_gc(T, LUA_GCCOLLECT, 0);
    if (failed && ctx->frees > before_frees)
      saw_partial_cleanup = 1;
    check(L, ctx->bad_osize == 0 && ctx->missing_ptr == 0,
	  "table allocator failure preserves allocator block sizes");
    check(L, ctx->live_blocks == before_live,
	  "table allocator failure releases partial allocations");
  }
  lua_settop(T, 0);
  check(L, saw_table_failure, "table allocator failure exercised");
  check(L, saw_partial_cleanup,
	"table partial allocation cleanup exercised");
}

static void test_table_shrink_allocator_failure(lua_State *L, lua_State *T,
						StrictAllocCtx *ctx)
{
  int limit;
  int saw_shrink_failure = 0;
  int saw_partial_cleanup = 0;
  int status;
  lua_gc(T, LUA_GCCOLLECT, 0);
  lua_settop(T, 0);
  status = luaL_loadbufferx(T, lua54_table_shrink_failure_chunk,
			    sizeof(lua54_table_shrink_failure_chunk) - 1u,
			    "=strict-table-shrink-failure", "t");
  check(L, status == LUA_OK, "table shrink allocator failure probe load");
  for (limit = 1; limit <= 2; limit++) {
    int before_live;
    int before_fails;
    int before_frees;
    int failed;
    int ok;
    int old_ok;
    lua_Integer new_seen;
    lua_gc(T, LUA_GCCOLLECT, 0);
    before_live = ctx->live_blocks;
    before_fails = ctx->call_fails;
    before_frees = ctx->frees;
    lua_pushvalue(T, 1);
    lua_pushinteger(T, limit);
    status = lua_pcall(T, 1, 3, 0);
    ctx->fail_at_alloc = 0;
    ctx->fail_once_alloc = 0;
    check(L, status == LUA_OK,
	  "table shrink allocator failure returns status tuple");
    ok = lua_toboolean(T, -3);
    old_ok = lua_toboolean(T, -2);
    new_seen = lua_tointeger(T, -1);
    failed = !ok;
    if (failed) {
      saw_shrink_failure = 1;
      check(L, ctx->call_fails > before_fails,
	    "table shrink allocator failure must come from allocator");
      check(L, old_ok && new_seen == 0,
	    "table shrink allocation failure keeps old table state");
    } else {
      check(L, new_seen == 32,
	    "table shrink allocator success inserts all hash keys");
      check(L, ctx->call_fails == before_fails,
	    "table shrink allocator success must not hide allocator failure");
    }
    lua_settop(T, 1);
    lua_gc(T, LUA_GCCOLLECT, 0);
    lua_gc(T, LUA_GCCOLLECT, 0);
    if (failed && ctx->frees > before_frees)
      saw_partial_cleanup = 1;
    check(L, ctx->bad_osize == 0 && ctx->missing_ptr == 0,
	  "table shrink allocator failure preserves block sizes");
    check(L, ctx->live_blocks == before_live,
	  "table shrink allocator failure releases partial allocations");
  }
  lua_settop(T, 0);
  check(L, saw_shrink_failure,
	"table shrink allocator failure exercised");
  check(L, saw_partial_cleanup,
	"table shrink allocator failure cleanup exercised");
}

static void test_jit_allocator_trace_flush(lua_State *L, lua_State *T,
					   StrictAllocCtx *ctx)
{
  int before_frees;
  int status;
  lua_gc(T, LUA_GCCOLLECT, 0);
  before_frees = ctx->frees;
  status = luaL_dostring(T,
    "local okjit, jitmod = pcall(require, 'jit')\n"
    "local okopt, jitopt = pcall(require, 'jit.opt')\n"
    "local okutil, jutil = pcall(require, 'jit.util')\n"
    "if not (okjit and okopt and okutil) then return 'skip' end\n"
    "local function trace_highwater()\n"
    "  local n = 0\n"
    "  for i = 1, 256 do if jutil.traceinfo(i) then n = i end end\n"
    "  return n\n"
    "end\n"
    "jitmod.off(trace_highwater, true)\n"
    "jitmod.on()\n"
    "jitmod.flush()\n"
    "jitopt.start('hotloop=1', 'hotexit=1')\n"
    "local before = trace_highwater()\n"
    "local sum = 0\n"
    "for round = 1, 4 do\n"
    "  for i = 1, 200 do sum = sum + i end\n"
    "end\n"
    "assert(trace_highwater() > before, 'strict allocator JIT trace')\n"
    "jitmod.flush()\n"
    "collectgarbage('collect')\n"
    "collectgarbage('collect')\n"
    "jitopt.start('hotloop=56', 'hotexit=10')\n"
    "return sum\n");
  check(L, status == LUA_OK, "strict allocator JIT trace status");
  if (lua_type(T, -1) == LUA_TSTRING) {
    check_string(L, -1, "skip", "strict allocator JIT skip marker");
  } else {
    check(L, lua_tonumber(T, -1) == (lua_Number)80400,
	  "strict allocator JIT trace result");
    check(L, ctx->frees > before_frees,
	  "strict allocator JIT trace flush frees trace memory");
  }
  lua_pop(T, 1);
  lua_gc(T, LUA_GCCOLLECT, 0);
  check(L, ctx->bad_osize == 0 && ctx->missing_ptr == 0,
	"strict allocator JIT trace preserves block sizes");
}

static void test_jit_allocator_record_failure(lua_State *L, lua_State *T,
					      StrictAllocCtx *ctx)
{
  static const char probe_chunk[] =
    "return function()\n"
    "  local sum = 0\n"
    "  for round = 1, 6 do\n"
    "    for i = 1, 180 do sum = sum + i end\n"
    "  end\n"
    "  return sum\n"
    "end\n";
  int limit;
  int saw_jit_failure = 0;
  int saw_partial_cleanup = 0;
  int status = luaL_dostring(T,
    "local okjit, jitmod = pcall(require, 'jit')\n"
    "local okopt, jitopt = pcall(require, 'jit.opt')\n"
    "if not (okjit and okopt) then return 'skip' end\n"
    "jitmod.on()\n"
    "jitmod.flush()\n"
    "jitopt.start('hotloop=1', 'hotexit=1')\n"
    "return true\n");
  check(L, status == LUA_OK, "strict allocator JIT failure setup status");
  if (lua_type(T, -1) == LUA_TSTRING) {
    check_string(L, -1, "skip",
		 "strict allocator JIT failure skip marker");
    lua_pop(T, 1);
    return;
  }
  lua_pop(T, 1);

  for (limit = 1; limit <= 16; limit++) {
    int before_fails;
    int before_frees;
    int before_live;
    int failed;
    status = luaL_dostring(T,
      "local jitmod = require('jit')\n"
      "local jitopt = require('jit.opt')\n"
      "jitmod.on()\n"
      "jitmod.flush()\n"
      "jitopt.start('hotloop=1', 'hotexit=1')\n"
      "return true\n");
    check(L, status == LUA_OK,
	  "strict allocator JIT failure iteration setup");
    lua_pop(T, 1);

    status = luaL_loadbufferx(T, probe_chunk, sizeof(probe_chunk) - 1u,
			      "=strict-jit-alloc-fail", "t");
    check(L, status == LUA_OK, "strict allocator JIT probe load");
    status = lua_pcall(T, 0, 1, 0);
    check(L, status == LUA_OK, "strict allocator JIT probe factory");

    before_live = ctx->live_blocks;
    before_frees = ctx->frees;
    before_fails = ctx->call_fails;
    ctx->fail_once_alloc = 1;
    ctx->fail_at_alloc = ctx->alloc_requests + limit;
    lua_pushvalue(T, -1);
    status = lua_pcall(T, 0, 1, 0);
    ctx->fail_at_alloc = 0;
    ctx->fail_once_alloc = 0;
    check(L, status == LUA_OK,
	  "JIT allocator failure aborts trace without Lua error");
    check(L, lua_tonumber(T, -1) == (lua_Number)97740,
	  "JIT allocator failure preserves interpreter result");
    failed = ctx->call_fails > before_fails;
    if (failed)
      saw_jit_failure = 1;
    lua_settop(T, 0);
    lua_gc(T, LUA_GCCOLLECT, 0);
    lua_gc(T, LUA_GCCOLLECT, 0);
    if (failed && ctx->frees > before_frees)
      saw_partial_cleanup = 1;
    check(L, ctx->live_blocks <= before_live,
	  "JIT allocator failure does not leak live blocks");
    check(L, ctx->bad_osize == 0,
	  "JIT allocator failure preserves block sizes");
    check(L, ctx->missing_ptr == 0,
	  "JIT allocator failure releases known pointers");
  }

  status = luaL_dostring(T,
    "local jitmod = require('jit')\n"
    "local jitopt = require('jit.opt')\n"
    "jitmod.flush()\n"
    "jitmod.on()\n"
    "jitopt.start('hotloop=56', 'hotexit=10')\n"
    "return true\n");
  check(L, status == LUA_OK, "strict allocator JIT failure reset");
  lua_pop(T, 1);
  check(L, saw_jit_failure, "JIT allocator failure exercised");
  check(L, saw_partial_cleanup, "JIT allocator failure cleanup exercised");
}

static void test_state_allocator_api(lua_State *L)
{
  AllocCtx ctx = { 0, 0 };
  SwitchAllocCtx switch_ctx = { 0, 0, 0, 0 };
  TrackingAllocCtx track_ctx = { 0, 0, 0 };
  ShrinkFailAllocCtx shrink_ctx = { 0, 0, 0, 0, 0 };
  StrictAllocCtx strict_ctx;
  StrictAllocCtx strict_fail_ctx;
  size_t strict_big = (size_t)LUAL_BUFFERSIZE * 24u;
  size_t strict_fail_big = (size_t)LUAL_BUFFERSIZE * 16u;
  size_t addvalue_big = (size_t)LUAL_BUFFERSIZE * 18u;
  size_t strict_len = 0;
  const char *strict_str;
  void *ud = NULL;
  lua_Alloc allocf;
  int status;
  lua_State *T;
  test_newstate_allocator_failure(L);
  test_newmetatable_allocator_failure(L);
  test_string_allocator_failure(L);
  check(L, lua_newstate(NULL, NULL) == NULL,
	"lua_newstate rejects NULL allocator");
  lua_pushcfunction(L, setallocf_null_function);
  status = lua_pcall(L, 0, 0, 0);
  check(L, status == LUA_ERRRUN, "lua_setallocf rejects NULL allocator");
  check(L, strstr(lua_tostring(L, -1), "invalid value") != NULL,
	"lua_setallocf NULL allocator error");
  lua_pop(L, 1);
  T = lua_newstate(counting_alloc, &ctx);
  check(L, T != NULL, "lua_newstate custom allocator");
  check(L, lua_atpanic(T, panic_a) == NULL, "lua_atpanic initial handler");
  check(L, lua_atpanic(T, panic_b) == panic_a, "lua_atpanic old handler");
  allocf = lua_getallocf(T, &ud);
  check(L, allocf == counting_alloc && ud == &ctx,
	"lua_getallocf custom allocator");
  check(L, lua_getallocf(T, NULL) == counting_alloc,
	"lua_getallocf accepts NULL userdata out parameter");
  lua_setallocf(T, allocf, ud);
  allocf = lua_getallocf(T, &ud);
  check(L, allocf == counting_alloc && ud == &ctx,
	"lua_setallocf preserves allocator");
  lua_setallocf(T, switching_alloc, &switch_ctx);
  allocf = lua_getallocf(T, &ud);
  check(L, allocf == switching_alloc && ud == &switch_ctx,
	"lua_setallocf switches allocator and userdata");
  luaL_openlibs(T);
  status = luaL_dostring(T,
    "local t = {}\n"
    "for i = 1, 512 do t[i] = ('allocator-switch-' .. i):rep(2) end\n"
    "_G.lua54_alloc_switch_t = t\n"
    "return true\n");
  check(L, status == LUA_OK, "switched allocator handles later allocations");
  lua_close(T);
  check(L, ctx.calls > 0 && ctx.frees > 0,
	"lua_close uses custom allocator");
  check(L, switch_ctx.calls > 0 && switch_ctx.allocs > 0 &&
	   switch_ctx.frees > 0,
	"lua_close uses switched allocator");

  close_call_count = 0;
  close_nil_error_count = 0;
  T = luaL_newstate();
  check(L, T != NULL, "lua_close toclose state");
  push_closeable(T, record_close);
  lua_toclose(T, -1);
  /* Lua 5.4 lua_close() must run active to-be-closed slots on the main
  ** thread; os.exit(..., true) depends on the same state-close path.
  */
  lua_close(T);
  check(L, close_call_count == 1 && close_nil_error_count == 1,
	"lua_close runs to-be-closed stack slots");

  T = lua_newstate(tracking_alloc, &track_ctx);
  check(L, T != NULL, "lua_newstate tracking allocator");
  lua_pushcfunction(T, raise_after_big_buffer);
  status = lua_pcall(T, 0, 0, 0);
  check(L, status == LUA_ERRRUN, "luaL_Buffer abort status");
  lua_pop(T, 1);
  lua_close(T);
  check(L, track_ctx.peak > (size_t)LUAL_BUFFERSIZE * 32u,
	"luaL_Buffer abort test must allocate a large buffer");
  check(L, track_ctx.live == 0,
	"luaL_Buffer large allocation is closed after error");

  T = lua_newstate(shrink_fail_alloc, &shrink_ctx);
  check(L, T != NULL, "lua_newstate shrink-fail allocator");
  luaL_openlibs(T);
  lua_pushcfunction(T, enable_shrink_fail_alloc);
  lua_setglobal(T, "enable_shrink_fail_alloc");
  status = luaL_dostring(T,
    "local function grow(n)\n"
    "  if n == 0 then return 0 end\n"
    "  return grow(n - 1) + 1\n"
    "end\n"
    "assert(grow(600) == 600)\n"
    "enable_shrink_fail_alloc()\n"
    "assert(#string.rep('x', 2 * 1024 * 1024) == 2 * 1024 * 1024)\n"
    "collectgarbage('collect')\n"
    "collectgarbage('collect')\n"
    "return true\n");
  shrink_ctx.fail_shrink = 0;
  check(L, shrink_ctx.shrink_fails > 0,
	"allocator test must exercise shrink failure");
  check(L, shrink_ctx.max_failed_shrink_osize > (size_t)(1024 * 1024),
	"allocator test must exercise tmpbuf shrink failure");
  check(L, status == LUA_OK,
	"GC opportunistic shrink allocator failure is non-fatal");
  lua_close(T);

  memset(&strict_fail_ctx, 0, sizeof(strict_fail_ctx));
  strict_fail_ctx.capacity = 1024;
  strict_fail_ctx.fail_grow_min_nsize = strict_fail_big * 2u;
  strict_fail_ctx.blocks =
    (StrictAllocBlock *)calloc((size_t)strict_fail_ctx.capacity,
			       sizeof(StrictAllocBlock));
  check(L, strict_fail_ctx.blocks != NULL,
	"strict fail allocator bookkeeping");
  T = lua_newstate(strict_alloc, &strict_fail_ctx);
  check(L, T != NULL, "lua_newstate strict fail allocator");
  strict_fail_ctx.fail_grow = 1;
  lua_pushcfunction(T, fail_growing_buffer);
  status = lua_pcall(T, 0, 0, 0);
  strict_fail_ctx.fail_grow = 0;
  check(L, status == LUA_ERRRUN,
	"luaL_Buffer failed grow reports runtime error");
  check(L, lua_tostring(T, -1) != NULL &&
	   strstr(lua_tostring(T, -1), "not enough memory") != NULL,
	"luaL_Buffer failed grow error message");
  check(L, strict_fail_ctx.grow_fails > 0 &&
	   strict_fail_ctx.max_failed_grow_nsize >= strict_fail_big * 2u,
	"luaL_Buffer failed grow exercises allocator");
  lua_settop(T, 0);
  check(L, !strict_alloc_has_block_at_least(&strict_fail_ctx, strict_fail_big),
	"luaL_Buffer failed grow closes old side buffer");
  lua_close(T);
  check(L, strict_fail_ctx.bad_osize == 0 &&
	   strict_fail_ctx.missing_ptr == 0,
	"luaL_Buffer failed grow preserves allocator block sizes");
  check(L, strict_fail_ctx.live_blocks == 0,
	"strict fail allocator releases all blocks");
  free(strict_fail_ctx.blocks);

  memset(&strict_ctx, 0, sizeof(strict_ctx));
  strict_ctx.capacity = 32768;
  strict_ctx.fail_shrink_min_osize = 32u * 1024u;
  strict_ctx.blocks = (StrictAllocBlock *)calloc((size_t)strict_ctx.capacity,
						 sizeof(StrictAllocBlock));
  check(L, strict_ctx.blocks != NULL, "strict allocator bookkeeping");
  T = lua_newstate(strict_alloc, &strict_ctx);
  check(L, T != NULL, "lua_newstate strict allocator");
  luaL_openlibs(T);
  lua_pushcfunction(T, return_big_buffer);
  status = lua_pcall(T, 0, 1, 0);
  check(L, status == LUA_OK, "strict allocator luaL_Buffer status");
  strict_str = lua_tolstring(T, -1, &strict_len);
  check(L, strict_str != NULL && strict_len == strict_big &&
	   strict_str[0] == 's' && strict_str[strict_big - 1] == 's',
	"strict allocator luaL_Buffer result");
  lua_pop(T, 1);
  lua_gc(T, LUA_GCCOLLECT, 0);
  lua_gc(T, LUA_GCCOLLECT, 0);
  check(L, !strict_alloc_has_block_at_least(&strict_ctx, addvalue_big),
	"strict allocator luaL_addvalue starts without large buffer");
  lua_pushcfunction(T, return_big_addvalue_buffer);
  status = lua_pcall(T, 0, 1, 0);
  check(L, status == LUA_OK, "strict allocator luaL_addvalue status");
  strict_str = lua_tolstring(T, -1, &strict_len);
  check(L, strict_str != NULL &&
	   strict_len == addvalue_big &&
	   strict_str[0] == 'v' &&
	   strict_str[strict_len - 1] == 'v',
	"strict allocator luaL_addvalue large result");
  lua_pop(T, 1);
  lua_gc(T, LUA_GCCOLLECT, 0);
  lua_gc(T, LUA_GCCOLLECT, 0);
  check(L, !strict_alloc_has_block_at_least(&strict_ctx, addvalue_big),
	"strict allocator luaL_addvalue closes side buffer");
  lua_pushcfunction(T, enable_strict_shrink_fail_alloc);
  lua_setglobal(T, "enable_strict_shrink_fail_alloc");
  lua_pushcfunction(T, strict_fail_once_after_alloc);
  lua_setglobal(T, "strict_fail_once_after_alloc");
  status = luaL_dostring(T,
    "local t = {}\n"
    "for i = 1, 8192 do t[i] = i end\n"
    "for i = 65, 8192 do t[i] = nil end\n"
    "enable_strict_shrink_fail_alloc()\n"
    "for i = 1, 8192 do t['strict-alloc-' .. i] = i end\n"
    "for i = 1, 64 do assert(t[i] == i) end\n"
    "for i = 65, 8192 do assert(t[i] == nil) end\n"
    "for i = 1, 8192 do assert(t['strict-alloc-' .. i] == i) end\n"
    "return true\n");
  strict_ctx.fail_shrink = 0;
  check(L, status == LUA_OK,
	"table repartition must tolerate allocator refusing shrink");
  check(L, strict_ctx.shrink_fails == 0,
	"table repartition must not use in-place shrink");
  test_createtable_allocator_failure(L, T, &strict_ctx);
  test_table_shrink_allocator_failure(L, T, &strict_ctx);
  test_table_allocator_failure(L, T, &strict_ctx);
  test_newthread_allocator_failure(L, T, &strict_ctx);
  test_pushcclosure_allocator_failure(L, T, &strict_ctx);
  test_newuserdatauv_allocator_failure(L, T, &strict_ctx);
  test_parser_allocator_failure(L, T, &strict_ctx);
  test_parser_exact_array_allocator_failure(L, T, &strict_ctx);
  test_jit_allocator_trace_flush(L, T, &strict_ctx);
  test_jit_allocator_record_failure(L, T, &strict_ctx);
  lua_close(T);
  check(L, strict_ctx.bad_osize == 0 && strict_ctx.missing_ptr == 0,
	"strict allocator preserves block sizes");
  check(L, strict_ctx.live_blocks == 0,
	"strict allocator releases all table repartition blocks");
  free(strict_ctx.blocks);
}

static int checkinteger_fraction(lua_State *L)
{
  luaL_checkinteger(L, 1);
  return 0;
}

static int checkinteger_arg(lua_State *L)
{
  lua_pushinteger(L, luaL_checkinteger(L, 1));
  return 1;
}

static int checknumber_arg(lua_State *L)
{
  lua_pushnumber(L, luaL_checknumber(L, 1));
  return 1;
}

static int checkudata_arg(lua_State *L)
{
  luaL_checkudata(L, 1, "capi.ud");
  return 0;
}

static int optnumber_arg(lua_State *L)
{
  lua_pushnumber(L, luaL_optnumber(L, 1, (lua_Number)3.25));
  return 1;
}

static int raise_lua_error(lua_State *L)
{
  lua_pushliteral(L, "capi raised error");
  return lua_error(L);
}

static int error_missing_object(lua_State *L)
{
  return lua_error(L);
}

static int optinteger_fraction(lua_State *L)
{
  luaL_optinteger(L, 1, 0);
  return 0;
}

static int optinteger_arg(lua_State *L)
{
  lua_Integer def = (lua_Integer)2048 * 1024 * 1024;
  lua_pushinteger(L, luaL_optinteger(L, 1, def));
  return 1;
}

static int len_meta(lua_State *L)
{
  (void)L;
  lua_pushinteger(L, 77);
  return 1;
}

static int len_wide_meta(lua_State *L)
{
  lua_Integer big40 = (lua_Integer)1024 * 1024 * 1024 * 1024;
  lua_pushnumber(L, (lua_Number)big40);
  return 1;
}

static int len_bad_meta(lua_State *L)
{
  (void)L;
  lua_pushliteral(L, "bad-len");
  return 1;
}

static int len_fraction_meta(lua_State *L)
{
  (void)L;
  lua_pushnumber(L, (lua_Number)1.5);
  return 1;
}

static int push_fraction_len_userdata(lua_State *L)
{
  (void)lua_newuserdatauv(L, 1, 0);
  lua_newtable(L);
  lua_pushcfunction(L, len_fraction_meta);
  lua_setfield(L, -2, "__len");
  lua_setmetatable(L, -2);
  return 1;
}

static int newuserdatauv_negative_nuv(lua_State *L)
{
  (void)lua_newuserdatauv(L, 1, -1);
  return 1;
}

static int newuserdatauv_large_nuv(lua_State *L)
{
  (void)lua_newuserdatauv(L, 1, SHRT_MAX);
  return 1;
}

static int getsubtable_index_meta(lua_State *L)
{
  const char *key = luaL_checkstring(L, 2);
  if (strcmp(key, "virtual") == 0) {
    lua_newtable(L);
    lua_pushliteral(L, "from-index");
    lua_setfield(L, -2, "origin");
    return 1;
  }
  return 0;
}

static int getsubtable_newindex_meta(lua_State *L)
{
  lua_pushvalue(L, 2);
  lua_setfield(L, lua_upvalueindex(1), "key");
  lua_pushvalue(L, 3);
  lua_setfield(L, lua_upvalueindex(1), "captured");
  return 0;
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

static int require_open_false(lua_State *L)
{
  check_string(L, 1, "capi.falsemod",
	       "luaL_requiref passes false-loaded module name");
  require_open_count++;
  lua_newtable(L);
  lua_pushliteral(L, "false-ready");
  lua_setfield(L, -2, "state");
  return 1;
}

static int require_open_nil(lua_State *L)
{
  check_string(L, 1, "capi.nilmod",
	       "luaL_requiref passes nil-returning module name");
  require_open_count++;
  lua_pushnil(L);
  return 1;
}

static int checkversion_bad_version(lua_State *L)
{
  luaL_checkversion_(L, LUA_VERSION_NUM - 1, LUAL_NUMSIZES);
  return 0;
}

static int checkversion_bad_sizes(lua_State *L)
{
  luaL_checkversion_(L, LUA_VERSION_NUM, LUAL_NUMSIZES + 1);
  return 0;
}

static int push_answer(lua_State *L)
{
  lua_pushinteger(L, 42);
  return 1;
}

static int call_missing_function(lua_State *L)
{
  lua_call(L, 0, 0);
  return 0;
}

static int call_negative_nargs(lua_State *L)
{
  lua_pushcfunction(L, push_answer);
  lua_call(L, -1, 0);
  return 0;
}

static int call_invalid_nresults(lua_State *L)
{
  lua_pushcfunction(L, push_answer);
  lua_call(L, 0, LUA_MULTRET - 1);
  return 0;
}

static int pcall_missing_function(lua_State *L)
{
  lua_pcall(L, 0, 0, 0);
  return 0;
}

static int pcall_negative_nargs(lua_State *L)
{
  lua_pushcfunction(L, push_answer);
  lua_pcall(L, -1, 0, 0);
  return 0;
}

static int pcall_invalid_nresults(lua_State *L)
{
  lua_pushcfunction(L, push_answer);
  lua_pcall(L, 0, LUA_MULTRET - 1, 0);
  return 0;
}

static int compare_invalid_op(lua_State *L)
{
  lua_pushinteger(L, 1);
  lua_pushinteger(L, 2);
  lua_compare(L, -2, -1, LUA_OPLE + 1);
  return 0;
}

static int compare_invalid_index_invalid_op(lua_State *L)
{
  lua_pushboolean(L, lua_compare(L, 1, 2, LUA_OPLE + 1));
  return 1;
}

static int pushcclosure_negative_upvalues(lua_State *L)
{
  lua_pushcclosure(L, push_answer, -1);
  return 1;
}

static int pushcclosure_large_upvalues(lua_State *L)
{
  lua_pushcclosure(L, push_answer, (int)UCHAR_MAX + 1);
  return 1;
}

static int pushcclosure_missing_upvalue(lua_State *L)
{
  lua_pushcclosure(L, push_answer, 1);
  return 1;
}

static int pushcclosure_null_function(lua_State *L)
{
  lua_pushcclosure(L, NULL, 0);
  return 1;
}

static int getinfo_null_what(lua_State *L)
{
  lua_Debug ar;
  lua_getinfo(L, NULL, &ar);
  return 0;
}

static int getinfo_null_debug(lua_State *L)
{
  lua_getinfo(L, "S", NULL);
  return 0;
}

static int getstack_null_debug(lua_State *L)
{
  lua_getstack(L, 0, NULL);
  return 0;
}

static int setlocal_null_debug(lua_State *L)
{
  lua_pushnil(L);
  lua_setlocal(L, NULL, 1);
  return 0;
}

static int setlocal_missing_name_preserves_stack(lua_State *L)
{
  lua_Debug ar;
  int top;
  check(L, lua_getstack(L, 0, &ar), "lua_setlocal missing-name setup");
  lua_pushliteral(L, "sentinel");
  top = lua_gettop(L);
  check(L, lua_setlocal(L, &ar, 9999) == NULL,
	"lua_setlocal missing name returns NULL");
  check(L, lua_gettop(L) == top, "lua_setlocal missing name preserves stack");
  lua_pop(L, 1);
  return 0;
}

static int xmove_negative_count(lua_State *L)
{
  lua_State *co = lua_newthread(L);
  lua_xmove(L, co, -1);
  return 0;
}

static int xmove_large_count(lua_State *L)
{
  lua_State *co = lua_newthread(L);
  lua_xmove(L, co, lua_gettop(L) + 1);
  return 0;
}

static lua_State *xmove_cross_state_target;

static int xmove_cross_state(lua_State *L)
{
  xmove_cross_state_target = luaL_newstate();
  if (xmove_cross_state_target == NULL)
    lua_error(L);
  lua_xmove(L, xmove_cross_state_target, 0);
  return 0;
}

static int settop_too_negative(lua_State *L)
{
  lua_settop(L, -2);
  return 0;
}

static int settop_too_large(lua_State *L)
{
  lua_settop(L, INT_MAX);
  return 0;
}

static int pushvalue_invalid_index(lua_State *L)
{
  lua_pushvalue(L, 1);
  return 0;
}

static int len_invalid_index(lua_State *L)
{
  lua_len(L, 1);
  return 1;
}

static int copy_invalid_from_index(lua_State *L)
{
  lua_pushnil(L);
  lua_copy(L, 2, 1);
  return 0;
}

static int copy_invalid_to_index(lua_State *L)
{
  lua_pushnil(L);
  lua_copy(L, 1, 2);
  return 0;
}

static int remove_invalid_index(lua_State *L)
{
  lua_remove(L, 1);
  return 0;
}

static int insert_invalid_index(lua_State *L)
{
  lua_pushnil(L);
  lua_insert(L, -2);
  return 0;
}

static int rotate_invalid_index(lua_State *L)
{
  lua_pushnil(L);
  lua_rotate(L, -2, 1);
  return 0;
}

static int rotate_large_count(lua_State *L)
{
  lua_pushnil(L);
  lua_pushnil(L);
  lua_rotate(L, -2, 3);
  return 0;
}

static int rotate_min_count(lua_State *L)
{
  lua_pushnil(L);
  lua_rotate(L, -1, INT_MIN);
  return 0;
}

static int typename_too_low(lua_State *L)
{
  (void)lua_typename(L, LUA_TNONE - 1);
  return 0;
}

static int typename_too_high(lua_State *L)
{
  (void)lua_typename(L, LUA_NUMTYPES + 3);
  return 0;
}

static int pcall_invalid_errfunc(lua_State *L)
{
  lua_pushcfunction(L, push_answer);
  lua_pcall(L, 0, 0, 2);
  return 0;
}

static int pcall_nonfunction_errfunc(lua_State *L)
{
  lua_pushboolean(L, 1);
  lua_pushcfunction(L, raise_lua_error);
  lua_pcall(L, 0, 0, 1);
  return 0;
}

static int concat_negative_count(lua_State *L)
{
  lua_concat(L, -1);
  return 1;
}

static int concat_large_count(lua_State *L)
{
  lua_concat(L, lua_gettop(L) + 1);
  return 0;
}

static int replace_missing_value(lua_State *L)
{
  lua_replace(L, LUA_REGISTRYINDEX);
  return 0;
}

static int settable_missing_pair(lua_State *L)
{
  lua_settable(L, LUA_REGISTRYINDEX);
  return 0;
}

static int setfield_missing_value(lua_State *L)
{
  lua_setfield(L, LUA_REGISTRYINDEX, "lua54_missing_value");
  return 0;
}

static int seti_missing_value(lua_State *L)
{
  lua_seti(L, LUA_REGISTRYINDEX, 1);
  return 0;
}

static int rawset_missing_pair(lua_State *L)
{
  lua_rawset(L, LUA_REGISTRYINDEX);
  return 0;
}

static int rawseti_missing_value(lua_State *L)
{
  lua_rawseti_sig(L, LUA_REGISTRYINDEX, 1);
  return 0;
}

static int rawseti54_missing_value(lua_State *L)
{
  lua_rawseti_sig(L, LUA_REGISTRYINDEX,
		  (lua_Integer)1024 * 1024 * 1024 * 1024);
  return 0;
}

static int rawsetp_missing_value(lua_State *L)
{
  lua_rawsetp(L, LUA_REGISTRYINDEX, (const void *)rawsetp_missing_value);
  return 0;
}

static int rawget_missing_key(lua_State *L)
{
  lua_rawget(L, LUA_REGISTRYINDEX);
  return 0;
}

static int rawget_non_table(lua_State *L)
{
  lua_pushinteger(L, 1);
  lua_pushliteral(L, "k");
  lua_rawget(L, -2);
  return 0;
}

static int rawgeti_non_table(lua_State *L)
{
  lua_pushinteger(L, 1);
  lua_rawgeti_sig(L, -1, 1);
  return 0;
}

static int rawgeti54_non_table(lua_State *L)
{
  lua_pushinteger(L, 1);
  lua_rawgeti_sig(L, -1, (lua_Integer)1024 * 1024 * 1024 * 1024);
  return 0;
}

static int rawgetp_non_table(lua_State *L)
{
  lua_pushinteger(L, 1);
  lua_rawgetp(L, -1, (const void *)rawgetp_non_table);
  return 0;
}

static int next_missing_key(lua_State *L)
{
  lua_next(L, LUA_REGISTRYINDEX);
  return 0;
}

static int next_non_table(lua_State *L)
{
  lua_pushinteger(L, 1);
  lua_pushnil(L);
  lua_next(L, -2);
  return 0;
}

static int rawset_non_table(lua_State *L)
{
  lua_pushinteger(L, 1);
  lua_pushliteral(L, "k");
  lua_pushliteral(L, "v");
  lua_rawset(L, -3);
  return 0;
}

static int rawseti_non_table(lua_State *L)
{
  lua_pushinteger(L, 1);
  lua_pushliteral(L, "v");
  lua_rawseti_sig(L, -2, 1);
  return 0;
}

static int rawseti54_non_table(lua_State *L)
{
  lua_pushinteger(L, 1);
  lua_pushliteral(L, "v");
  lua_rawseti_sig(L, -2, (lua_Integer)1024 * 1024 * 1024 * 1024);
  return 0;
}

static int rawsetp_non_table(lua_State *L)
{
  lua_pushinteger(L, 1);
  lua_pushliteral(L, "v");
  lua_rawsetp(L, -2, (const void *)rawsetp_non_table);
  return 0;
}

static int setmetatable_missing_value(lua_State *L)
{
  lua_setmetatable(L, LUA_REGISTRYINDEX);
  return 0;
}

static int setmetatable_nontable_value(lua_State *L)
{
  lua_pushinteger(L, 1);
  lua_pushboolean(L, 1);
  lua_setmetatable(L, -2);
  return 0;
}

static int setiuservalue_missing_value(lua_State *L)
{
  lua_setiuservalue(L, LUA_REGISTRYINDEX, 1);
  return 0;
}

static int setupvalue_missing_value_inner(lua_State *L)
{
  lua_setupvalue(L, lua_upvalueindex(1), 1);
  return 0;
}

static int setupvalue_missing_value(lua_State *L)
{
  lua_pushcfunction(L, push_answer);
  lua_pushcclosure(L, setupvalue_missing_value_inner, 1);
  lua_call(L, 0, 0);
  return 0;
}

static int getupvalue_invalid_index(lua_State *L)
{
  (void)lua_getupvalue(L, 1, 1);
  return 0;
}

static int setupvalue_invalid_index(lua_State *L)
{
  lua_pushnil(L);
  (void)lua_setupvalue(L, -2, 1);
  return 0;
}

static void push_one_upvalue_closure(lua_State *L)
{
  int status = luaL_loadstring(L,
    "local x = 1\n"
    "return function() return x end");
  if (status != LUA_OK)
    lua_error(L);
  lua_call(L, 0, 1);
}

static int upvalueid_invalid_index(lua_State *L)
{
  (void)lua_upvalueid(L, 1, 1);
  return 0;
}

static int upvalueid_non_function(lua_State *L)
{
  lua_pushboolean(L, 1);
  (void)lua_upvalueid(L, -1, 1);
  return 0;
}

static int upvaluejoin_non_function_first(lua_State *L)
{
  lua_pushboolean(L, 1);
  push_one_upvalue_closure(L);
  lua_upvaluejoin(L, -2, 1, -1, 1);
  return 0;
}

static int upvaluejoin_non_function_second(lua_State *L)
{
  push_one_upvalue_closure(L);
  lua_pushboolean(L, 1);
  lua_upvaluejoin(L, -2, 1, -1, 1);
  return 0;
}

static int upvaluejoin_bad_first_upvalue(lua_State *L)
{
  push_one_upvalue_closure(L);
  push_one_upvalue_closure(L);
  lua_upvaluejoin(L, -2, 2, -1, 1);
  return 0;
}

static int upvaluejoin_bad_second_upvalue(lua_State *L)
{
  push_one_upvalue_closure(L);
  push_one_upvalue_closure(L);
  lua_upvaluejoin(L, -2, 1, -1, 2);
  return 0;
}

static int gettable_missing_key(lua_State *L)
{
  lua_gettable(L, LUA_REGISTRYINDEX);
  return 0;
}

static int gettable_invalid_index(lua_State *L)
{
  lua_pushliteral(L, "key");
  lua_gettable(L, -2);
  return 0;
}

static int getfield_invalid_index(lua_State *L)
{
  lua_getfield(L, 1, "key");
  return 0;
}

static int getfield_null_name(lua_State *L)
{
  lua_newtable(L);
  lua_getfield_sig(L, -1, NULL);
  return 0;
}

static int getglobal_null_name(lua_State *L)
{
  lua_getglobal_sig(L, NULL);
  return 0;
}

static int settable_invalid_index(lua_State *L)
{
  lua_pushliteral(L, "key");
  lua_pushliteral(L, "value");
  lua_settable(L, -3);
  return 0;
}

static int setfield_invalid_index(lua_State *L)
{
  lua_pushliteral(L, "value");
  lua_setfield(L, -2, "key");
  return 0;
}

static int setfield_null_name(lua_State *L)
{
  lua_newtable(L);
  lua_pushnil(L);
  lua_setfield(L, -2, NULL);
  return 0;
}

static int setglobal_null_name(lua_State *L)
{
  lua_pushnil(L);
  lua_setglobal_sig(L, NULL);
  return 0;
}

static int newmetatable_null_name(lua_State *L)
{
  luaL_newmetatable(L, NULL);
  return 0;
}

static int getmetafield_null_name(lua_State *L)
{
  lua_newtable(L);
  luaL_getmetafield(L, -1, NULL);
  return 0;
}

static int callmeta_null_name(lua_State *L)
{
  lua_newtable(L);
  luaL_callmeta(L, -1, NULL);
  return 0;
}

static int callmeta_invalid_index(lua_State *L)
{
  luaL_callmeta(L, 1, "__tostring");
  return 0;
}

static int setmetatable_laux_null_name(lua_State *L)
{
  lua_newtable(L);
  luaL_setmetatable(L, NULL);
  return 0;
}

static int testudata_null_name(lua_State *L)
{
  lua_newuserdatauv(L, 1, 0);
  luaL_testudata(L, -1, NULL);
  return 0;
}

static int testudata_invalid_index(lua_State *L)
{
  luaL_testudata(L, 1, "capi.ud");
  return 0;
}

static int checkudata_null_name(lua_State *L)
{
  lua_newuserdatauv(L, 1, 0);
  luaL_checkudata(L, -1, NULL);
  return 0;
}

static int checkudata_invalid_index(lua_State *L)
{
  luaL_checkudata(L, 1, "capi.ud");
  return 0;
}

static int getsubtable_null_name(lua_State *L)
{
  lua_newtable(L);
  luaL_getsubtable(L, -1, NULL);
  return 0;
}

static int requiref_null_name(lua_State *L)
{
  luaL_requiref(L, NULL, require_open, 0);
  return 0;
}

static int requiref_null_openf(lua_State *L)
{
  luaL_requiref(L, "capi.nullopen", NULL, 0);
  return 0;
}

static int setmetatable_invalid_object_index(lua_State *L)
{
  lua_newtable(L);
  lua_setmetatable(L, -2);
  return 0;
}

static int getiuservalue_invalid_index(lua_State *L)
{
  lua_getiuservalue(L, 1, 1);
  return 0;
}

static int setiuservalue_invalid_index(lua_State *L)
{
  lua_pushnil(L);
  lua_setiuservalue(L, -2, 1);
  return 0;
}

static void check_fresh_invalid_value(lua_State *L, lua_CFunction fn,
				      const char *statusmsg,
				      const char *errmsg)
{
  lua_State *T = luaL_newstate();
  int status;
  check(L, T != NULL, statusmsg);
  lua_pushcfunction(T, fn);
  status = lua_pcall(T, 0, 0, 0);
  check(L, status == LUA_ERRRUN, statusmsg);
  check(L, strstr(lua_tostring(T, -1), "invalid value") != NULL, errmsg);
  lua_close(T);
}

static void check_xmove_cross_state(lua_State *L)
{
  lua_State *T = luaL_newstate();
  int status;
  check(L, T != NULL, "lua_xmove cross-state setup");
  xmove_cross_state_target = NULL;
  lua_pushcfunction(T, xmove_cross_state);
  status = lua_pcall(T, 0, 0, 0);
  check(L, status == LUA_ERRRUN, "lua_xmove rejects cross-state move");
  check(L, strstr(lua_tostring(T, -1), "invalid value") != NULL,
	"lua_xmove cross-state error");
  if (xmove_cross_state_target != NULL) {
    lua_close(xmove_cross_state_target);
    xmove_cross_state_target = NULL;
  }
  lua_close(T);
}

static void check_resume_invalid_value(lua_State *L, lua_CFunction fn,
				       const char *statusmsg,
				       const char *errmsg)
{
  lua_State *co = lua_newthread(L);
  int nres = -1;
  int status;
  lua_pushcfunction(co, fn);
  status = lua_resume_sig(co, L, 0, &nres);
  check(L, status == LUA_ERRRUN, statusmsg);
  check(L, strstr(lua_tostring(co, -1), "invalid value") != NULL, errmsg);
  lua_pop(L, 1);
}

static void check_yieldk_cont_invalid_value(lua_State *L, lua_CFunction fn,
					    const char *yieldmsg,
					    const char *statusmsg,
					    const char *errmsg)
{
  lua_State *co = lua_newthread(L);
  int nres = -1;
  int status;
  lua_pushcfunction(co, fn);
  status = lua_resume_sig(co, L, 0, &nres);
  check(L, status == LUA_YIELD, yieldmsg);
  status = lua_resume_sig(co, L, 0, &nres);
  check(L, status == LUA_ERRRUN, statusmsg);
  check(L, strstr(lua_tostring(co, -1), "invalid value") != NULL, errmsg);
  lua_pop(L, 1);
}

static int capi_gc_reentry(lua_State *L)
{
  lua_pushinteger(L, lua_gc(L, LUA_GCCOUNT));
  lua_pushinteger(L, lua_gc(L, LUA_GCGEN, 0, 0));
  return 2;
}

static int capi_transfer_cfunc(lua_State *L)
{
  check_integer(L, 1, 1, "capi transfer C arg 1");
  check_integer(L, 2, 2, "capi transfer C arg 2");
  check_integer(L, 3, 3, "capi transfer C arg 3");
  lua_pushinteger(L, 6);
  return 1;
}

static int record_close(lua_State *L)
{
  close_call_count++;
  if (lua_gettop(L) == 2 && lua_isnil(L, 2))
    close_nil_error_count++;
  if (lua_gettop(L) == 2 && lua_tostring(L, 2) != NULL &&
      strstr(lua_tostring(L, 2), "capi body boom") != NULL)
    close_body_error_count++;
  return 0;
}

static int record_close_error(lua_State *L)
{
  close_call_count++;
  if (lua_gettop(L) == 2 && lua_isnil(L, 2))
    close_nil_error_count++;
  lua_pushliteral(L, "capi close boom");
  return lua_error(L);
}

static int record_inner_close_error(lua_State *L)
{
  close_call_count++;
  if (lua_gettop(L) == 2 && lua_tostring(L, 2) != NULL &&
      strstr(lua_tostring(L, 2), "capi inner close boom") != NULL)
    close_body_error_count++;
  return 0;
}

static void push_closeable(lua_State *L, lua_CFunction closef)
{
  lua_newtable(L);
  lua_newtable(L);
  lua_pushcfunction(L, closef);
  lua_setfield(L, -2, "__close");
  lua_setmetatable(L, -2);
}

static void push_lua_yielding_closeable(lua_State *L, const char *yield_value,
					const char *error_after_yield)
{
  lua_newtable(L);
  lua_newtable(L);
  lua_pushfstring(L,
    "return function(_, err) "
    "assert(err == nil); "
    /* Keep this generated Lua source on %s: the close-yield cases below
    ** should exercise __close scheduling, not formatter edge cases.
    */
    "coroutine.yield('%s'); "
    "%s "
    "end",
    yield_value,
    error_after_yield != NULL ? error_after_yield : "return true");
  check(L, luaL_loadstring(L, lua_tostring(L, -1)) == LUA_OK,
	"lua yielding __close load");
  lua_remove(L, -2);
  lua_call(L, 0, 1);
  lua_setfield(L, -2, "__close");
  lua_setmetatable(L, -2);
}

static int push_closeable_userdata(lua_State *L)
{
  int *ud = (int *)lua_newuserdatauv(L, sizeof(int), 0);
  *ud = 54;
  lua_newtable(L);
  lua_pushcfunction(L, record_close);
  lua_setfield(L, -2, "__close");
  lua_setmetatable(L, -2);
  return 1;
}

static int push_plain_userdata(lua_State *L)
{
  int *ud = (int *)lua_newuserdatauv(L, sizeof(int), 0);
  *ud = 54;
  return 1;
}

static int mark_userdata_close_then_return(lua_State *L)
{
  push_closeable_userdata(L);
  lua_toclose(L, -1);
  return 0;
}

static int mark_nonclosable_slot(lua_State *L)
{
  lua_pushinteger(L, 1);
  lua_toclose(L, -1);
  return 0;
}

static int mark_false_then_closeslot(lua_State *L)
{
  lua_pushboolean(L, 0);
  lua_toclose(L, -1);
  lua_closeslot(L, -1);
  return 0;
}

static int mark_close_then_pop(lua_State *L)
{
  push_closeable(L, record_close);
  lua_toclose(L, -1);
  lua_settop(L, 0);
  return 0;
}

static int mark_close_twice_same_slot(lua_State *L)
{
  push_closeable(L, record_close);
  lua_toclose(L, -1);
  lua_toclose(L, -1);
  return 0;
}

static int mark_close_below_active_slot(lua_State *L)
{
  push_closeable(L, record_close);
  push_closeable(L, record_close);
  lua_toclose(L, -1);
  lua_toclose(L, -2);
  return 0;
}

static int mark_nil_below_active_slot(lua_State *L)
{
  lua_pushnil(L);
  push_closeable(L, record_close);
  lua_toclose(L, -1);
  lua_toclose(L, -2);
  return 0;
}

static int closeslot_not_last(lua_State *L)
{
  push_closeable(L, record_close);
  lua_toclose(L, -1);
  push_closeable(L, record_close);
  lua_toclose(L, -1);
  lua_closeslot(L, -2);
  return 0;
}

static int closeslot_lifo(lua_State *L)
{
  push_closeable(L, record_close);
  lua_toclose(L, -1);
  push_closeable(L, record_close);
  lua_toclose(L, -1);
  lua_closeslot(L, -1);
  lua_closeslot(L, -2);
  return 0;
}

static int mark_close_then_return(lua_State *L)
{
  push_closeable(L, record_close);
  lua_toclose(L, -1);
  return 0;
}

static int mark_close_then_return_self(lua_State *L)
{
  push_closeable(L, record_close);
  lua_toclose(L, -1);
  return 1;
}

static int mark_lua_close_yield_then_return(lua_State *L)
{
  push_lua_yielding_closeable(L, "capi close yield", NULL);
  lua_toclose(L, -1);
  lua_pushliteral(L, "capi return after close yield");
  return 1;
}

static int mark_lua_close_yield_error_then_return(lua_State *L)
{
  push_lua_yielding_closeable(L, "capi close yield before error",
			      "error('capi close yield boom', 0)");
  lua_toclose(L, -1);
  lua_pushliteral(L, "unreachable after close yield error");
  return 1;
}

static int mark_two_lua_close_yield_then_return(lua_State *L)
{
  push_lua_yielding_closeable(L, "capi outer close yield", NULL);
  lua_toclose(L, -1);
  push_lua_yielding_closeable(L, "capi inner close yield", NULL);
  lua_toclose(L, -1);
  lua_pushliteral(L, "capi return after two close yields");
  return 1;
}

static int mark_lua_close_yield_then_return_self(lua_State *L)
{
  push_lua_yielding_closeable(L, "capi returned close yield", NULL);
  lua_toclose(L, -1);
  return 1;
}

static int mark_lua_close_yield_error_with_outer_close(lua_State *L)
{
  push_closeable(L, record_inner_close_error);
  lua_toclose(L, -1);
  push_lua_yielding_closeable(L, "capi inner close yield before error",
			      "error('capi inner close boom', 0)");
  lua_toclose(L, -1);
  lua_pushliteral(L, "unreachable after inner close yield error");
  return 1;
}

static int mark_close_error_then_return(lua_State *L)
{
  push_closeable(L, record_close_error);
  lua_toclose(L, -1);
  return 0;
}

static int mark_close_then_error(lua_State *L)
{
  push_closeable(L, record_close);
  lua_toclose(L, -1);
  return luaL_error(L, "capi body boom");
}

static int push_upvalue(lua_State *L)
{
  /* Lua 5.4 external headers use a registry-based upvalue pseudo-index; this
  ** exercises the runtime normalization back to LuaJIT's internal upvalue ABI.
  */
  lua_pushvalue(L, lua_upvalueindex(1));
  return 1;
}

static int capi_tostring_meta(lua_State *L)
{
  (void)L;
  lua_pushliteral(L, "meta tostring");
  return 1;
}

static int capi_metafield_index(lua_State *L)
{
  (void)L;
  lua_pushliteral(L, "indexed metafield");
  return 1;
}

static int checkoption_arg(lua_State *L)
{
  static const char *opts[] = { "alpha", "beta", "gamma", NULL };
  lua_pushinteger(L, luaL_checkoption(L, 1, "beta", opts));
  return 1;
}

static int checkoption_null_list(lua_State *L)
{
  lua_pushliteral(L, "alpha");
  luaL_checkoption(L, 1, NULL, NULL);
  return 0;
}

static int laux_string_macro_arg(lua_State *L)
{
  const char *required = luaL_checkstring(L, 1);
  const char *optional = luaL_optstring(L, 2, "fallback");
  lua_pushfstring(L, "%s/%s", required, optional);
  return 1;
}

static int laux_argcheck_fail(lua_State *L)
{
  /* Exercise the Lua 5.4 macro path, not only the exported luaL_argerror()
  ** function, because embedders compile these checks into their own modules.
  */
  luaL_argcheck(L, 0, 1, "macro guard failed");
  return 0;
}

static int laux_argexpected_fail(lua_State *L)
{
  luaL_argexpected(L, 0, 1, "macro-value");
  return 0;
}

static int laux_argexpected_table_arg(lua_State *L)
{
  luaL_argexpected(L, 0, 1, "table");
  return 0;
}

static int laux_argerror_fail(lua_State *L)
{
  return luaL_argerror(L, 1, "explicit laux failure");
}

static int laux_argerror_negative_arg(lua_State *L)
{
  return luaL_argerror(L, -1, "negative index failure");
}

static int laux_checktype_any_arg(lua_State *L)
{
  luaL_checkany(L, 1);
  luaL_checktype(L, 1, LUA_TTABLE);
  lua_pushliteral(L, "ok");
  return 1;
}

static int laux_checkthread_arg(lua_State *L)
{
  luaL_checktype(L, 1, LUA_TTHREAD);
  return 0;
}

static int laux_checktype_invalid_expected_low(lua_State *L)
{
  lua_pushnil(L);
  luaL_checktype(L, 1, LUA_TNONE - 1);
  return 0;
}

static int laux_checktype_invalid_expected_high(lua_State *L)
{
  lua_pushnil(L);
  luaL_checktype(L, 1, LUA_NUMTYPES + 3);
  return 0;
}

static int laux_opt_macro_arg(lua_State *L)
{
  lua_pushinteger(L, luaL_opt(L, luaL_checkinteger, 1, 77));
  return 1;
}

static int laux_len_arg(lua_State *L)
{
  lua_pushinteger(L, luaL_len(L, 1));
  return 1;
}

static int laux_ref_missing_value(lua_State *L)
{
  (void)luaL_ref(L, LUA_REGISTRYINDEX);
  return 0;
}

static int laux_checkstack_arg(lua_State *L)
{
  luaL_checkstack(L, LUA_MINSTACK, "laux stack guard");
  lua_pushliteral(L, "ok");
  return 1;
}

static int laux_checkstack_null_msg(lua_State *L)
{
  luaL_checkstack(L, INT_MAX, NULL);
  return 0;
}

static int laux_error_arg(lua_State *L)
{
  return luaL_error(L, "laux boom");
}

static int laux_error_wide_integer_arg(lua_State *L)
{
  lua_Integer big40 = (lua_Integer)1024 * 1024 * 1024 * 1024;
  return luaL_error(L, "laux big %I", big40);
}

static int traceback_null_thread(lua_State *L)
{
  luaL_traceback(L, NULL, "trace", 0);
  return 0;
}

static const luaL_Reg capi_newlib[] = {
  { "answer", push_answer },
  { NULL, NULL }
};

static int capi_newlib_checkversion_count;

static void capi_note_checkversion(lua_State *L)
{
  (void)L;
  capi_newlib_checkversion_count++;
}

/* The Lua 5.4 luaL_newlib macro is required to call luaL_checkversion().
** Redefine that macro only for this helper so the smoke can prove the macro
** shape without perturbing the real luaL_checkversion() runtime tests below.
*/
#undef luaL_checkversion
#define luaL_checkversion(L) capi_note_checkversion((L))
static void test_newlib_macro_checkversion(lua_State *L)
{
  capi_newlib_checkversion_count = 0;
  luaL_newlib(L, capi_newlib);
  check(L, capi_newlib_checkversion_count == 1,
	"luaL_newlib calls luaL_checkversion");
  lua_pop(L, 1);
}
#undef luaL_checkversion
#define luaL_checkversion(L) \
  luaL_checkversion_(L, LUA_VERSION_NUM, LUAL_NUMSIZES)

static const luaL_Reg capi_setfuncs[] = {
  { "upvalue", push_upvalue },
  { NULL, NULL }
};

static const luaL_Reg capi_setfuncs_placeholder[] = {
  { "upvalue", push_upvalue },
  { "placeholder", NULL },
  { NULL, NULL }
};

static int setfuncs_null_list(lua_State *L)
{
  lua_newtable(L);
  luaL_setfuncs(L, NULL, 0);
  return 0;
}

static int setfuncs_negative_upvalues(lua_State *L)
{
  lua_newtable(L);
  luaL_setfuncs(L, capi_setfuncs, -1);
  return 0;
}

static int setfuncs_missing_target(lua_State *L)
{
  luaL_setfuncs(L, capi_setfuncs, 0);
  return 0;
}

static int setfuncs_missing_upvalue(lua_State *L)
{
  lua_newtable(L);
  luaL_setfuncs(L, capi_setfuncs, 1);
  return 0;
}

static int yield_once(lua_State *L)
{
  return lua_yield(L, 0);
}

static int yield_two(lua_State *L)
{
  lua_pushliteral(L, "y1");
  lua_pushliteral(L, "y2");
  return lua_yieldk_sig(L, 2, 0, NULL);
}

static int yield_negative_results(lua_State *L)
{
  return lua_yield(L, -1);
}

static int yield_too_many_results(lua_State *L)
{
  return lua_yield(L, lua_gettop(L) + 1);
}

static int yieldk_negative_results(lua_State *L)
{
  return lua_yieldk_sig(L, -1, 0, NULL);
}

static int yieldk_negative_cont(lua_State *L, int status, lua_KContext ctx)
{
  (void)L;
  (void)status;
  (void)ctx;
  return -1;
}

static int yieldk_too_many_cont(lua_State *L, int status, lua_KContext ctx)
{
  (void)status;
  (void)ctx;
  return lua_gettop(L) + 1;
}

static int yield_with_negative_cont(lua_State *L)
{
  return lua_yieldk_sig(L, 0, 0, yieldk_negative_cont);
}

static int yield_with_too_many_cont(lua_State *L)
{
  return lua_yieldk_sig(L, 0, 0, yieldk_too_many_cont);
}

static int yieldk_cont_called;

static int yieldk_resume_cont(lua_State *L, int status, lua_KContext ctx)
{
  yieldk_cont_called++;
  check(L, status == LUA_YIELD, "lua_yieldk continuation status");
  check(L, ctx == (lua_KContext)0x54, "lua_yieldk continuation context");
  check_string(L, 1, "resume-arg", "lua_yieldk continuation resume arg");
  lua_pushliteral(L, "cont-result");
  return 1;
}

static int yield_with_cont(lua_State *L)
{
  lua_pushliteral(L, "yield-result");
  return lua_yieldk_sig(L, 1, (lua_KContext)0x54, yieldk_resume_cont);
}

static int yieldk_error_cont(lua_State *L, int status, lua_KContext ctx)
{
  check(L, status == LUA_YIELD, "lua_yieldk error continuation status");
  check(L, ctx == (lua_KContext)0x55, "lua_yieldk error continuation context");
  lua_pushliteral(L, "yieldk cont boom");
  return lua_error(L);
}

static int yield_with_error_cont(lua_State *L)
{
  lua_pushliteral(L, "yield-before-error");
  return lua_yieldk_sig(L, 1, (lua_KContext)0x55, yieldk_error_cont);
}

static int yieldk_reyield_cont_called;

static int yieldk_reyield_second_cont(lua_State *L, int status, lua_KContext ctx)
{
  yieldk_reyield_cont_called++;
  check(L, status == LUA_YIELD, "lua_yieldk reyield second status");
  check(L, ctx == (lua_KContext)0x57, "lua_yieldk reyield second context");
  check_string(L, 1, "resume-2", "lua_yieldk reyield second resume arg");
  lua_pushliteral(L, "reyield-final");
  return 1;
}

static int yieldk_reyield_first_cont(lua_State *L, int status, lua_KContext ctx)
{
  yieldk_reyield_cont_called++;
  check(L, status == LUA_YIELD, "lua_yieldk reyield first status");
  check(L, ctx == (lua_KContext)0x56, "lua_yieldk reyield first context");
  check_string(L, 1, "resume-1", "lua_yieldk reyield first resume arg");
  lua_pushliteral(L, "yield-from-cont");
  /* Lua 5.4 continuations are still yieldable C frames. Re-yielding here
  ** catches implementations that resume the continuation through a plain
  ** protected C call instead of preserving a VM continuation boundary.
  */
  return lua_yieldk_sig(L, 1, (lua_KContext)0x57, yieldk_reyield_second_cont);
}

static int yield_with_reyield_cont(lua_State *L)
{
  lua_pushliteral(L, "yield-before-cont");
  return lua_yieldk_sig(L, 1, (lua_KContext)0x56, yieldk_reyield_first_cont);
}

static int capi_unexpected_cont_called;

static int unexpected_capi_cont(lua_State *L, int status, lua_KContext ctx)
{
  (void)L;
  (void)status;
  (void)ctx;
  capi_unexpected_cont_called++;
  return 0;
}

static int callk_yield_cont_called;

static int callk_yield_cont(lua_State *L, int status, lua_KContext ctx)
{
  callk_yield_cont_called++;
  check(L, status == LUA_YIELD, "lua_callk yielding continuation status");
  check(L, ctx == (lua_KContext)0x6600,
	"lua_callk yielding continuation context");
  check(L, lua_gettop(L) == 1, "lua_callk yielding continuation stack");
  check_string(L, 1, "callk-resume",
	"lua_callk yielding continuation callee result");
  lua_pushliteral(L, "callk-cont-result");
  return 1;
}

static int callk_yield_driver(lua_State *L)
{
  check(L, luaL_loadstring(L,
	"return coroutine.yield('callk-yield')") == LUA_OK,
	"lua_callk yielding callee load");
  lua_callk_sig(L, 0, 1, (lua_KContext)0x6600, callk_yield_cont);
  /* If the callee does not yield, Lua 5.4 expects the original C function to
  ** continue normally; route through the same assertion helper with LUA_OK.
  */
  return callk_yield_cont(L, LUA_OK, (lua_KContext)0x6600);
}

static int pcallk_yield_cont_called;

static int pcallk_yield_cont(lua_State *L, int status, lua_KContext ctx)
{
  pcallk_yield_cont_called++;
  check(L, status == LUA_YIELD, "lua_pcallk yielding continuation status");
  check(L, ctx == (lua_KContext)0x6700,
	"lua_pcallk yielding continuation context");
  check(L, lua_gettop(L) == 1, "lua_pcallk yielding continuation stack");
  check_string(L, 1, "pcallk-resume",
	"lua_pcallk yielding continuation callee result");
  lua_pushliteral(L, "pcallk-cont-result");
  return 1;
}

static int pcallk_yield_driver(lua_State *L)
{
  int status;
  check(L, luaL_loadstring(L,
	"return coroutine.yield('pcallk-yield')") == LUA_OK,
	"lua_pcallk yielding callee load");
  status = lua_pcallk_sig(L, 0, 1, 0, (lua_KContext)0x6700,
			  pcallk_yield_cont);
  return pcallk_yield_cont(L, status, (lua_KContext)0x6700);
}

static int pcallk_yield_error_cont_called;

static int pcallk_yield_error_cont(lua_State *L, int status, lua_KContext ctx)
{
  pcallk_yield_error_cont_called++;
  check(L, status == LUA_ERRRUN, "lua_pcallk yield-error continuation status");
  check(L, ctx == (lua_KContext)0x6701,
	"lua_pcallk yield-error continuation context");
  check(L, lua_gettop(L) == 1, "lua_pcallk yield-error continuation stack");
  check_string(L, 1, "pcallk-error-after-yield",
	"lua_pcallk yield-error continuation error object");
  lua_pushliteral(L, "pcallk-error-handled");
  return 1;
}

static int pcallk_yield_error_driver(lua_State *L)
{
  int status;
  check(L, luaL_loadstring(L,
	"coroutine.yield('pcallk-error-yield'); "
	"error('pcallk-error-after-yield', 0)") == LUA_OK,
	"lua_pcallk yield-error callee load");
  status = lua_pcallk_sig(L, 0, 1, 0, (lua_KContext)0x6701,
			  pcallk_yield_error_cont);
  return pcallk_yield_error_cont(L, status, (lua_KContext)0x6701);
}

static int pcallk_msgh_yield_error_cont_called;
static int pcallk_msgh_error_yield_error_cont_called;

static int pcallk_msgh_yield_error_cont(lua_State *L, int status,
					lua_KContext ctx)
{
  pcallk_msgh_yield_error_cont_called++;
  check(L, status == LUA_ERRRUN,
	"lua_pcallk message handler yield-error continuation status");
  check(L, ctx == (lua_KContext)0x6705,
	"lua_pcallk message handler yield-error continuation context");
  check(L, lua_gettop(L) == 1,
	"lua_pcallk message handler yield-error continuation stack");
  check_string(L, 1, "pcallk-handled:pcallk-msgh-error-after-yield",
	"lua_pcallk message handler transformed error object");
  lua_pushliteral(L, "pcallk-msgh-error-handled");
  return 1;
}

static int pcallk_msgh_handler(lua_State *L)
{
  lua_pushliteral(L, "pcallk-handled:");
  lua_pushvalue(L, 1);
  lua_concat(L, 2);
  return 1;
}

static int pcallk_msgh_yield_error_driver(lua_State *L)
{
  int status;
  lua_pushcfunction(L, pcallk_msgh_handler);
  check(L, luaL_loadstring(L,
	"coroutine.yield('pcallk-msgh-error-yield'); "
	"error('pcallk-msgh-error-after-yield', 0)") == LUA_OK,
	"lua_pcallk message handler yield-error callee load");
  status = lua_pcallk_sig(L, 0, 1, 1, (lua_KContext)0x6705,
			  pcallk_msgh_yield_error_cont);
  return pcallk_msgh_yield_error_cont(L, status, (lua_KContext)0x6705);
}

static int pcallk_msgh_error_yield_error_cont(lua_State *L, int status,
					      lua_KContext ctx)
{
  pcallk_msgh_error_yield_error_cont_called++;
  check(L, status == LUA_ERRERR,
	"lua_pcallk message handler error continuation status");
  check(L, ctx == (lua_KContext)0x6706,
	"lua_pcallk message handler error continuation context");
  check(L, lua_gettop(L) == 1,
	"lua_pcallk message handler error continuation stack");
  check_string(L, 1, "error in error handling",
	"lua_pcallk message handler error object");
  lua_pushliteral(L, "pcallk-msgh-error-error-handled");
  return 1;
}

static int pcallk_msgh_error_handler(lua_State *L)
{
  return luaL_error(L, "pcallk-msgh-handler-boom");
}

static int pcallk_msgh_error_yield_error_driver(lua_State *L)
{
  int status;
  lua_pushcfunction(L, pcallk_msgh_error_handler);
  check(L, luaL_loadstring(L,
	"coroutine.yield('pcallk-msgh-error-handler-yield'); "
	"error('pcallk-msgh-error-handler-after-yield', 0)") == LUA_OK,
	"lua_pcallk message handler error callee load");
  status = lua_pcallk_sig(L, 0, 1, 1, (lua_KContext)0x6706,
			  pcallk_msgh_error_yield_error_cont);
  return pcallk_msgh_error_yield_error_cont(L, status,
					    (lua_KContext)0x6706);
}

static int callk_yield_error_cont_called;

static int callk_yield_error_cont(lua_State *L, int status, lua_KContext ctx)
{
  (void)L;
  (void)status;
  (void)ctx;
  callk_yield_error_cont_called++;
  return 0;
}

static int callk_yield_error_driver(lua_State *L)
{
  check(L, luaL_loadstring(L,
	"coroutine.yield('callk-error-yield'); "
	"error('callk-error-after-yield', 0)") == LUA_OK,
	"lua_callk yield-error callee load");
  lua_callk_sig(L, 0, 1, (lua_KContext)0x6603, callk_yield_error_cont);
  lua_pushliteral(L, "callk-error-unexpected-return");
  return 1;
}

static int callk_reyield_cont_called;

static int callk_reyield_second_cont(lua_State *L, int status, lua_KContext ctx)
{
  callk_reyield_cont_called++;
  check(L, status == LUA_YIELD, "lua_callk reyield second status");
  check(L, ctx == (lua_KContext)0x6602, "lua_callk reyield second context");
  check_string(L, 1, "callk-resume-2", "lua_callk reyield second resume arg");
  lua_pushliteral(L, "callk-reyield-final");
  return 1;
}

static int callk_reyield_first_cont(lua_State *L, int status, lua_KContext ctx)
{
  callk_reyield_cont_called++;
  check(L, status == LUA_YIELD, "lua_callk reyield first status");
  check(L, ctx == (lua_KContext)0x6601, "lua_callk reyield first context");
  check_string(L, 1, "callk-callee-final",
	"lua_callk reyield first callee result");
  lua_pushliteral(L, "callk-yield-from-cont");
  return lua_yieldk_sig(L, 1, (lua_KContext)0x6602,
			callk_reyield_second_cont);
}

static int callk_reyield_driver(lua_State *L)
{
  check(L, luaL_loadstring(L,
	"coroutine.yield('callk-callee-yield'); return 'callk-callee-final'") ==
	LUA_OK, "lua_callk reyield callee load");
  lua_callk_sig(L, 0, 1, (lua_KContext)0x6601, callk_reyield_first_cont);
  return callk_reyield_first_cont(L, LUA_OK, (lua_KContext)0x6601);
}

static int pcallk_reyield_cont_called;

static int pcallk_reyield_second_cont(lua_State *L, int status, lua_KContext ctx)
{
  pcallk_reyield_cont_called++;
  check(L, status == LUA_YIELD, "lua_pcallk reyield second status");
  check(L, ctx == (lua_KContext)0x6703, "lua_pcallk reyield second context");
  check_string(L, 1, "pcallk-resume-2",
	"lua_pcallk reyield second resume arg");
  lua_pushliteral(L, "pcallk-reyield-final");
  return 1;
}

static int pcallk_reyield_first_cont(lua_State *L, int status, lua_KContext ctx)
{
  pcallk_reyield_cont_called++;
  check(L, status == LUA_YIELD, "lua_pcallk reyield first status");
  check(L, ctx == (lua_KContext)0x6702, "lua_pcallk reyield first context");
  check_string(L, 1, "pcallk-callee-final",
	"lua_pcallk reyield first callee result");
  lua_pushliteral(L, "pcallk-yield-from-cont");
  return lua_yieldk_sig(L, 1, (lua_KContext)0x6703,
			pcallk_reyield_second_cont);
}

static int pcallk_reyield_driver(lua_State *L)
{
  int status;
  check(L, luaL_loadstring(L,
	"coroutine.yield('pcallk-callee-yield'); return 'pcallk-callee-final'") ==
	LUA_OK, "lua_pcallk reyield callee load");
  status = lua_pcallk_sig(L, 0, 1, 0, (lua_KContext)0x6702,
			  pcallk_reyield_first_cont);
  return pcallk_reyield_first_cont(L, status, (lua_KContext)0x6702);
}

static int callk_multret_cont_called;

static int callk_multret_cont(lua_State *L, int status, lua_KContext ctx)
{
  callk_multret_cont_called++;
  check(L, status == LUA_YIELD, "lua_callk multret continuation status");
  check(L, ctx == (lua_KContext)0x6604,
	"lua_callk multret continuation context");
  check(L, lua_gettop(L) == 3, "lua_callk multret continuation stack");
  check_string(L, 1, "callk-m1", "lua_callk multret result 1");
  check(L, lua_isnil(L, 2), "lua_callk multret nil hole");
  check_string(L, 3, "callk-m3", "lua_callk multret result 3");
  lua_pushliteral(L, "callk-multret-cont");
  return 1;
}

static int callk_multret_driver(lua_State *L)
{
  check(L, luaL_loadstring(L,
	"coroutine.yield('callk-multret-yield'); "
	"return 'callk-m1', nil, 'callk-m3'") == LUA_OK,
	"lua_callk multret callee load");
  lua_callk_sig(L, 0, LUA_MULTRET, (lua_KContext)0x6604,
		callk_multret_cont);
  return callk_multret_cont(L, LUA_OK, (lua_KContext)0x6604);
}

static int pcallk_multret_cont_called;

static int pcallk_multret_cont(lua_State *L, int status, lua_KContext ctx)
{
  pcallk_multret_cont_called++;
  check(L, status == LUA_YIELD, "lua_pcallk multret continuation status");
  check(L, ctx == (lua_KContext)0x6704,
	"lua_pcallk multret continuation context");
  check(L, lua_gettop(L) == 3, "lua_pcallk multret continuation stack");
  check_string(L, 1, "pcallk-m1", "lua_pcallk multret result 1");
  check(L, lua_isnil(L, 2), "lua_pcallk multret nil hole");
  check_string(L, 3, "pcallk-m3", "lua_pcallk multret result 3");
  lua_pushliteral(L, "pcallk-multret-cont");
  return 1;
}

static int pcallk_multret_driver(lua_State *L)
{
  int status;
  check(L, luaL_loadstring(L,
	"coroutine.yield('pcallk-multret-yield'); "
	"return 'pcallk-m1', nil, 'pcallk-m3'") == LUA_OK,
	"lua_pcallk multret callee load");
  status = lua_pcallk_sig(L, 0, LUA_MULTRET, 0, (lua_KContext)0x6704,
			  pcallk_multret_cont);
  return pcallk_multret_cont(L, status, (lua_KContext)0x6704);
}

static int push_isyieldable(lua_State *L)
{
  /* lua_isyieldable() depends on the currently running C frame, so check it
  ** inside a coroutine resumed through the Lua 5.4 lua_resume() surface.
  */
  lua_pushboolean(L, lua_isyieldable(L));
  return 1;
}

static int return_two(lua_State *L)
{
  lua_pushliteral(L, "r1");
  lua_pushliteral(L, "r2");
  return 2;
}

static void capture_warning(void *ud, const char *msg, int tocont)
{
  (void)ud;
  strncpy(warning_buf, msg, sizeof(warning_buf)-1);
  warning_buf[sizeof(warning_buf)-1] = '\0';
  warning_tocont = tocont;
}

static const char *capi_reader(lua_State *L, void *ud, size_t *sz)
{
  CApiReaderCtx *ctx = (CApiReaderCtx *)ud;
  const char *src = ctx->src;
  (void)L;
  if (src == NULL) {
    *sz = 0;
    return NULL;
  }
  *sz = ctx->len;
  ctx->src = NULL;
  ctx->len = 0;
  return src;
}

static int dump_writer(lua_State *L, const void *p, size_t sz, void *ud)
{
  DumpBuffer *b = (DumpBuffer *)ud;
  (void)L;
  if (b->len + sz > sizeof(b->data))
    return 1;
  memcpy(b->data + b->len, p, sz);
  b->len += sz;
  return 0;
}

static int dump_empty_stack(lua_State *L)
{
  DumpBuffer b;
  memset(&b, 0, sizeof(b));
  lua_dump_sig(L, dump_writer, &b, 0);
  return 0;
}

static int dump_null_writer(lua_State *L)
{
  check(L, luaL_loadstring(L, "return 54") == LUA_OK,
	"lua_dump NULL writer setup");
  lua_dump_sig(L, NULL, NULL, 0);
  return 0;
}

static int load_null_reader(lua_State *L)
{
  lua_load_sig(L, NULL, NULL, "=null-reader", "t");
  return 0;
}

static int stringtonumber_null_string(lua_State *L)
{
  lua_stringtonumber(L, NULL);
  return 0;
}

static int loadbuffer_null_nonzero(lua_State *L)
{
  luaL_loadbufferx(L, NULL, 1, "=null-buffer", "t");
  return 0;
}

static int loadstring_null_string(lua_State *L)
{
  luaL_loadstring(L, NULL);
  return 0;
}

static int addlstring_null_nonzero(lua_State *L)
{
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  luaL_addlstring(&b, NULL, 1);
  return 0;
}

static int addstring_null_string(lua_State *L)
{
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  luaL_addstring(&b, NULL);
  return 0;
}

static int buffer_prep_missing_placeholder(lua_State *L)
{
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  lua_pop(L, 1);
  (void)luaL_prepbuffsize(&b, 1);
  return 0;
}

static int buffer_prep_wrong_placeholder(lua_State *L)
{
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  lua_pop(L, 1);
  lua_pushlightuserdata(L, (void *)L);
  (void)luaL_prepbuffsize(&b, 1);
  return 0;
}

static int buffer_pushresult_missing_placeholder(lua_State *L)
{
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  lua_pop(L, 1);
  luaL_pushresult(&b);
  return 0;
}

static int buffer_addvalue_missing_value(lua_State *L)
{
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  luaL_addvalue(&b);
  return 0;
}

static int buffer_addvalue_bad_value(lua_State *L)
{
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  lua_pushboolean(L, 1);
  luaL_addvalue(&b);
  return 0;
}

static int addgsub_null_subject(lua_State *L)
{
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  luaL_addgsub(&b, NULL, "x", "y");
  return 0;
}

static int addgsub_null_pattern(lua_State *L)
{
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  luaL_addgsub(&b, "x", NULL, "y");
  return 0;
}

static int addgsub_null_replacement(lua_State *L)
{
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  luaL_addgsub(&b, "x", "x", NULL);
  return 0;
}

static int dump_fail_writer(lua_State *L, const void *p, size_t sz, void *ud)
{
  int *calls = (int *)ud;
  (void)L; (void)p; (void)sz;
  ++*calls;
  return 77;
}

static int pushlstring_null_nonzero(lua_State *L)
{
  lua_pushlstring(L, NULL, 1);
  return 1;
}

static int pushfstring_bad_format(lua_State *L)
{
  lua_pushfstring(L, "%Z");
  return 1;
}

static int pushfstring_bad_modifier(lua_State *L)
{
  lua_pushfstring(L, "%04d", 7);
  return 1;
}

static int pushfstring_bad_unsigned(lua_State *L)
{
  lua_pushfstring(L, "%u", 7);
  return 1;
}

static int pushfstring_bad_floatfmt(lua_State *L)
{
  lua_pushfstring(L, "%g", (lua_Number)1.5);
  return 1;
}

static int pushfstring_null_format(lua_State *L)
{
  lua_pushfstring(L, NULL);
  return 1;
}

static const char *pushvfstring_wrap(lua_State *L, const char *fmt, ...)
{
  const char *ret;
  va_list argp;
  va_start(argp, fmt);
  ret = lua_pushvfstring(L, fmt, argp);
  va_end(argp);
  return ret;
}

static int pushvfstring_null_format(lua_State *L)
{
  (void)pushvfstring_wrap(L, NULL);
  return 1;
}

static int arith_string_bor(lua_State *L)
{
  lua_pushliteral(L, "1099511627776");
  lua_pushinteger(L, 7);
  lua_arith(L, LUA_OPBOR);
  return 1;
}

static int arith_string_bnot(lua_State *L)
{
  lua_pushliteral(L, "7");
  lua_arith(L, LUA_OPBNOT);
  return 1;
}

static int arith_string_add_meta(lua_State *L)
{
  (void)L;
  lua_pushliteral(L, "api-add-meta");
  return 1;
}

static int arith_string_idiv_meta(lua_State *L)
{
  (void)L;
  lua_pushliteral(L, "api-idiv-meta");
  return 1;
}

static int arith_string_bor_meta(lua_State *L)
{
  (void)L;
  lua_pushliteral(L, "api-bor-meta");
  return 1;
}

static int arith_missing_binary_operand(lua_State *L)
{
  lua_pushinteger(L, 1);
  lua_arith(L, LUA_OPADD);
  return 1;
}

static int arith_missing_unary_operand(lua_State *L)
{
  lua_arith(L, LUA_OPUNM);
  return 1;
}

static int arith_invalid_op(lua_State *L)
{
  lua_pushinteger(L, 1);
  lua_pushinteger(L, 2);
  lua_arith(L, LUA_OPBNOT + 1);
  return 1;
}

static void test_stack_and_number_api(lua_State *L)
{
  static const char light_key;
  lua_Integer iv = 0;
  int okflag;
  int status;
  void **extra;
  const char *ret;
  const char with_nul[] = { 'a', '\0', 'b' };
  lua_State *co;
  void *fullud;

  lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_MAINTHREAD);
  check(L, lua_tothread(L, -1) == L, "LUA_RIDX_MAINTHREAD");
  lua_pop(L, 1);

  check(L, LUA_NUMTAGS == LUA_NUMTYPES, "LUA_NUMTAGS");
  check(L, strcmp(LUA_VERSION_RELEASE, "8") == 0, "LUA_VERSION_RELEASE");
  check(L, strcmp(LUA_RELEASE, "Lua 5.4.8") == 0, "LUA_RELEASE");
  check(L, strstr(LUA_COPYRIGHT, "Lua 5.4.8") != NULL, "LUA_COPYRIGHT release");
  check(L, strchr(LUA_AUTHORS, '&') == NULL, "LUA_AUTHORS separator");
  check(L, LUA_VERSION_RELEASE_NUM == 50408, "LUA_VERSION_RELEASE_NUM");
  check(L, strstr(lua_ident, "LuaVersion: Lua 5.4.8") != NULL, "lua_ident version");
  check(L, strstr(lua_ident, "LuaAuthors:") != NULL, "lua_ident authors");
  lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);
  check(L, lua_istable(L, -1), "LUA_RIDX_GLOBALS");
  lua_pop(L, 1);
  check(L, LUA_RIDX_LAST == LUA_RIDX_GLOBALS, "LUA_RIDX_LAST");
  check(L, strcmp(LUA_GNAME, "_G") == 0, "LUA_GNAME");
  check(L, strcmp(LUA_FILEHANDLE, "FILE*") == 0, "LUA_FILEHANDLE");
  check(L, LUAL_BUFFERSIZE == (int)(16 * sizeof(void *) * sizeof(lua_Number)),
	"LUAL_BUFFERSIZE Lua 5.4 formula");
  check(L, LUAI_MAXSTACK == (LUAI_IS32INT ? 1000000 : 15000),
	"LUAI_MAXSTACK Lua 5.4 formula");
  check(L, luai_likely(1) && !luai_likely(0), "luai_likely");
  check(L, luai_unlikely(1) && !luai_unlikely(0), "luai_unlikely");
  check(L, LUA_REGISTRYINDEX == (-LUAI_MAXSTACK - 1000),
	"LUA_REGISTRYINDEX Lua 5.4 formula");
  check(L, lua_upvalueindex(1) == (LUA_REGISTRYINDEX - 1),
	"lua_upvalueindex Lua 5.4 formula");
  check(L, strcmp(LUA_LOADED_TABLE, "_LOADED") == 0, "LUA_LOADED_TABLE");
  check(L, strcmp(LUA_PRELOAD_TABLE, "_PRELOAD") == 0, "LUA_PRELOAD_TABLE");
  check(L, strcmp(LUA_VERSUFFIX, "_5_4") == 0, "LUA_VERSUFFIX");

  lua_createtable(L, -1, -1);
  check(L, lua_istable(L, -1), "lua_createtable negative hints table");
  check(L, lua_rawlen(L, -1) == 0, "lua_createtable negative hints length");
  lua_pop(L, 1);
  lua_createtable(L, 0, -1);
  check(L, lua_istable(L, -1), "lua_createtable negative hash hint table");
  check(L, lua_rawlen(L, -1) == 0,
	"lua_createtable negative hash hint length");
  lua_pop(L, 1);
  lua_createtable(L, -1, 0);
  check(L, lua_istable(L, -1), "lua_createtable negative array hint table");
  check(L, lua_rawlen(L, -1) == 0,
	"lua_createtable negative array hint length");
  lua_pop(L, 1);

  check(L, lua_type(L, lua_gettop(L) + 1) == LUA_TNONE,
	"lua_type none");
  check(L, lua_isnone(L, lua_gettop(L) + 1), "lua_isnone none");
  check(L, lua_isnoneornil(L, lua_gettop(L) + 1),
	"lua_isnoneornil none");
  check(L, strcmp(lua_typename(L, LUA_TNONE), "no value") == 0,
	"lua_typename none");
  check(L, strcmp(lua_typename(L, LUA_TNIL), "nil") == 0,
	"lua_typename nil");
  check_fresh_invalid_value(L, typename_too_low,
			    "lua_typename rejects below LUA_TNONE",
			    "lua_typename below LUA_TNONE error");
  check_fresh_invalid_value(L, typename_too_high,
			    "lua_typename rejects too-large type code",
			    "lua_typename too-large type code error");

  lua_pushnil(L);
  check(L, lua_type(L, -1) == LUA_TNIL, "lua_type nil");
  check(L, lua_isnil(L, -1), "lua_isnil nil");
  check(L, lua_isnoneornil(L, -1), "lua_isnoneornil nil");
  check(L, !lua_toboolean(L, -1), "lua_toboolean nil");
  lua_pop(L, 1);

  lua_pushboolean(L, 0);
  lua_pushboolean(L, 1);
  check(L, lua_isboolean(L, -2) && lua_isboolean(L, -1),
	"lua_isboolean values");
  check(L, !lua_toboolean(L, -2) && lua_toboolean(L, -1),
	"lua_toboolean booleans");
  check(L, !lua_rawequal(L, -2, -1), "lua_rawequal booleans false");
  lua_pop(L, 2);

  lua_pushliteral(L, "same");
  lua_pushliteral(L, "same");
  check(L, lua_rawequal(L, -2, -1), "lua_rawequal strings");
  lua_pop(L, 2);

  lua_pushinteger(L, 123);
  check(L, lua_isstring(L, -1), "lua_isstring number");
  check(L, lua_toboolean(L, -1), "lua_toboolean number");
  lua_pop(L, 1);

  lua_pushcfunction(L, push_answer);
  check(L, lua_iscfunction(L, -1), "lua_iscfunction c closure");
  check(L, lua_tocfunction(L, -1) == push_answer,
	"lua_tocfunction c closure");
  check(L, lua_topointer(L, -1) != NULL, "lua_topointer c closure");
  lua_pop(L, 1);

  lua_pushcfunction(L, pushcclosure_negative_upvalues);
  status = lua_pcall(L, 0, 0, 0);
  check(L, status == LUA_ERRRUN, "lua_pushcclosure rejects negative upvalues");
  check(L, strstr(lua_tostring(L, -1), "invalid value") != NULL,
	"lua_pushcclosure negative upvalues error");
  lua_pop(L, 1);

  lua_pushcfunction(L, pushcclosure_large_upvalues);
  status = lua_pcall(L, 0, 0, 0);
  check(L, status == LUA_ERRRUN, "lua_pushcclosure rejects large upvalues");
  check(L, strstr(lua_tostring(L, -1), "invalid value") != NULL,
	"lua_pushcclosure large upvalues error");
  lua_pop(L, 1);

  lua_pushcfunction(L, pushcclosure_missing_upvalue);
  status = lua_pcall(L, 0, 0, 0);
  check(L, status == LUA_ERRRUN, "lua_pushcclosure rejects missing upvalue");
  check(L, strstr(lua_tostring(L, -1), "invalid value") != NULL,
	"lua_pushcclosure missing upvalue error");
  lua_pop(L, 1);

  lua_pushcfunction(L, pushcclosure_null_function);
  status = lua_pcall(L, 0, 0, 0);
  check(L, status == LUA_ERRRUN, "lua_pushcclosure rejects NULL function");
  check(L, strstr(lua_tostring(L, -1), "invalid value") != NULL,
	"lua_pushcclosure NULL function error");
  lua_pop(L, 1);

  status = luaL_loadstring(L, "return 1");
  check(L, status == LUA_OK, "luaL_loadstring lua function setup");
  check(L, lua_isfunction(L, -1), "lua_isfunction lua closure");
  check(L, !lua_iscfunction(L, -1), "lua_iscfunction lua closure false");
  check(L, lua_tocfunction(L, -1) == NULL, "lua_tocfunction lua closure");
  lua_pop(L, 1);

  lua_pushlightuserdata(L, (void *)&light_key);
  check(L, lua_type(L, -1) == LUA_TLIGHTUSERDATA,
	"lua_type lightuserdata");
  check(L, lua_isuserdata(L, -1), "lua_isuserdata lightuserdata");
  check(L, lua_islightuserdata(L, -1), "lua_islightuserdata");
  check(L, lua_touserdata(L, -1) == (void *)&light_key,
	"lua_touserdata lightuserdata");
  check(L, lua_topointer(L, -1) == (void *)&light_key,
	"lua_topointer lightuserdata");
  lua_pop(L, 1);

  fullud = lua_newuserdatauv(L, 8, 0);
  check(L, lua_type(L, -1) == LUA_TUSERDATA, "lua_type userdata");
  check(L, lua_isuserdata(L, -1), "lua_isuserdata full userdata");
  check(L, !lua_islightuserdata(L, -1),
	"lua_islightuserdata full userdata false");
  check(L, lua_touserdata(L, -1) == fullud, "lua_touserdata userdata");
  check(L, lua_topointer(L, -1) != NULL, "lua_topointer userdata");
  lua_pop(L, 1);

  check(L, lua_pushthread(L) == 1, "lua_pushthread main return");
  check(L, lua_tothread(L, -1) == L, "lua_pushthread main value");
  lua_pop(L, 1);

  check(L, lua_checkstack(L, 8), "lua_checkstack grows stack");
  check(L, !lua_checkstack(L, -1), "lua_checkstack rejects negative size");
  lua_pushcfunction(L, settop_too_negative);
  status = lua_pcall(L, 0, 0, 0);
  check(L, status == LUA_ERRRUN, "lua_settop rejects too-negative index");
  check(L, strstr(lua_tostring(L, -1), "invalid value") != NULL,
	"lua_settop too-negative index error");
  lua_pop(L, 1);

  lua_pushcfunction(L, settop_too_large);
  status = lua_pcall(L, 0, 0, 0);
  check(L, status == LUA_ERRRUN, "lua_settop rejects too-large index");
  check(L, strstr(lua_tostring(L, -1), "invalid value") != NULL,
	"lua_settop too-large index error");
  lua_pop(L, 1);

  lua_pushcfunction(L, xmove_negative_count);
  status = lua_pcall(L, 0, 0, 0);
  check(L, status == LUA_ERRRUN, "lua_xmove rejects negative count");
  check(L, strstr(lua_tostring(L, -1), "invalid value") != NULL,
	"lua_xmove negative count error");
  lua_pop(L, 1);

  lua_pushcfunction(L, xmove_large_count);
  status = lua_pcall(L, 0, 0, 0);
  check(L, status == LUA_ERRRUN, "lua_xmove rejects large count");
  check(L, strstr(lua_tostring(L, -1), "invalid value") != NULL,
	"lua_xmove large count error");
  lua_pop(L, 1);
  check_xmove_cross_state(L);

  check_fresh_invalid_value(L, replace_missing_value,
			    "lua_replace rejects missing value",
			    "lua_replace missing value error");
  check_fresh_invalid_value(L, settable_missing_pair,
			    "lua_settable rejects missing key/value",
			    "lua_settable missing key/value error");
  check_fresh_invalid_value(L, setfield_missing_value,
			    "lua_setfield rejects missing value",
			    "lua_setfield missing value error");
  check_fresh_invalid_value(L, seti_missing_value,
			    "lua_seti rejects missing value",
			    "lua_seti missing value error");
  check_fresh_invalid_value(L, pushvalue_invalid_index,
			    "lua_pushvalue rejects invalid index",
			    "lua_pushvalue invalid index error");
  check_fresh_invalid_value(L, len_invalid_index,
			    "lua_len rejects invalid index",
			    "lua_len invalid index error");
  check_fresh_invalid_value(L, copy_invalid_from_index,
			    "lua_copy rejects invalid source index",
			    "lua_copy invalid source index error");
  check_fresh_invalid_value(L, copy_invalid_to_index,
			    "lua_copy rejects invalid destination index",
			    "lua_copy invalid destination index error");
  check_fresh_invalid_value(L, remove_invalid_index,
			    "lua_remove rejects invalid index",
			    "lua_remove invalid index error");
  check_fresh_invalid_value(L, insert_invalid_index,
			    "lua_insert rejects invalid index",
			    "lua_insert invalid index error");
  check_fresh_invalid_value(L, rotate_invalid_index,
			    "lua_rotate rejects invalid index",
			    "lua_rotate invalid index error");
  check_fresh_invalid_value(L, rotate_large_count,
			    "lua_rotate rejects too-large count",
			    "lua_rotate too-large count error");
  check_fresh_invalid_value(L, rotate_min_count,
			    "lua_rotate rejects INT_MIN count",
			    "lua_rotate INT_MIN count error");
  check_fresh_invalid_value(L, pcall_invalid_errfunc,
			    "lua_pcall rejects invalid errfunc index",
			    "lua_pcall invalid errfunc index error");
  check_fresh_invalid_value(L, pcall_nonfunction_errfunc,
			    "lua_pcall rejects non-function errfunc",
			    "lua_pcall non-function errfunc error");
  check_fresh_invalid_value(L, compare_invalid_op,
			    "lua_compare rejects invalid op",
			    "lua_compare invalid op error");
  lua_pushcfunction(L, compare_invalid_index_invalid_op);
  status = lua_pcall(L, 0, 1, 0);
  check(L, status == LUA_OK, "lua_compare invalid index short-circuits");
  check(L, !lua_toboolean(L, -1),
	"lua_compare invalid index returns false before op check");
  lua_pop(L, 1);
  check_fresh_invalid_value(L, rawget_missing_key,
			    "lua_rawget rejects missing key",
			    "lua_rawget missing key error");
  check_fresh_invalid_value(L, rawget_non_table,
			    "lua_rawget rejects non-table",
			    "lua_rawget non-table error");
  check_fresh_invalid_value(L, rawgeti_non_table,
			    "lua_rawgeti rejects non-table",
			    "lua_rawgeti non-table error");
  check_fresh_invalid_value(L, rawgeti54_non_table,
			    "lua_rawgeti rejects wide-key non-table",
			    "lua_rawgeti wide-key non-table error");
  check_fresh_invalid_value(L, rawgetp_non_table,
			    "lua_rawgetp rejects non-table",
			    "lua_rawgetp non-table error");
  check_fresh_invalid_value(L, next_missing_key,
			    "lua_next rejects missing key",
			    "lua_next missing key error");
  check_fresh_invalid_value(L, next_non_table,
			    "lua_next rejects non-table",
			    "lua_next non-table error");
  check_fresh_invalid_value(L, rawset_missing_pair,
			    "lua_rawset rejects missing key/value",
			    "lua_rawset missing key/value error");
  check_fresh_invalid_value(L, rawset_non_table,
			    "lua_rawset rejects non-table",
			    "lua_rawset non-table error");
  check_fresh_invalid_value(L, rawseti_missing_value,
			    "lua_rawseti rejects missing value",
			    "lua_rawseti missing value error");
  check_fresh_invalid_value(L, rawseti_non_table,
			    "lua_rawseti rejects non-table",
			    "lua_rawseti non-table error");
  check_fresh_invalid_value(L, rawseti54_missing_value,
			    "lua_rawseti rejects missing wide-key value",
			    "lua_rawseti missing wide-key value error");
  check_fresh_invalid_value(L, rawseti54_non_table,
			    "lua_rawseti rejects wide-key non-table",
			    "lua_rawseti wide-key non-table error");
  check_fresh_invalid_value(L, rawsetp_missing_value,
			    "lua_rawsetp rejects missing value",
			    "lua_rawsetp missing value error");
  check_fresh_invalid_value(L, rawsetp_non_table,
			    "lua_rawsetp rejects non-table",
			    "lua_rawsetp non-table error");
  check_fresh_invalid_value(L, setmetatable_missing_value,
			    "lua_setmetatable rejects missing value",
			    "lua_setmetatable missing value error");
  check_fresh_invalid_value(L, setmetatable_nontable_value,
			    "lua_setmetatable rejects non-table metatable",
			    "lua_setmetatable non-table metatable error");
  check_fresh_invalid_value(L, setiuservalue_missing_value,
			    "lua_setiuservalue rejects missing value",
			    "lua_setiuservalue missing value error");
  check_fresh_invalid_value(L, setupvalue_missing_value,
			    "lua_setupvalue rejects missing value",
			    "lua_setupvalue missing value error");
  check_fresh_invalid_value(L, getupvalue_invalid_index,
			    "lua_getupvalue rejects invalid function index",
			    "lua_getupvalue invalid function index error");
  check_fresh_invalid_value(L, setupvalue_invalid_index,
			    "lua_setupvalue rejects invalid function index",
			    "lua_setupvalue invalid function index error");
  check_fresh_invalid_value(L, upvalueid_invalid_index,
			    "lua_upvalueid rejects invalid function index",
			    "lua_upvalueid invalid function index error");
  check_fresh_invalid_value(L, upvalueid_non_function,
			    "lua_upvalueid rejects non-function",
			    "lua_upvalueid non-function error");
  check_fresh_invalid_value(L, upvaluejoin_non_function_first,
			    "lua_upvaluejoin rejects non-function target",
			    "lua_upvaluejoin non-function target error");
  check_fresh_invalid_value(L, upvaluejoin_non_function_second,
			    "lua_upvaluejoin rejects non-function source",
			    "lua_upvaluejoin non-function source error");
  check_fresh_invalid_value(L, upvaluejoin_bad_first_upvalue,
			    "lua_upvaluejoin rejects bad target upvalue",
			    "lua_upvaluejoin bad target upvalue error");
  check_fresh_invalid_value(L, upvaluejoin_bad_second_upvalue,
			    "lua_upvaluejoin rejects bad source upvalue",
			    "lua_upvaluejoin bad source upvalue error");
  check_fresh_invalid_value(L, gettable_missing_key,
			    "lua_gettable rejects missing key",
			    "lua_gettable missing key error");
  check_fresh_invalid_value(L, gettable_invalid_index,
			    "lua_gettable rejects invalid table index",
			    "lua_gettable invalid table index error");
  check_fresh_invalid_value(L, getfield_invalid_index,
			    "lua_getfield rejects invalid table index",
			    "lua_getfield invalid table index error");
  check_fresh_invalid_value(L, getfield_null_name,
			    "lua_getfield rejects NULL name",
			    "lua_getfield NULL name error");
  check_fresh_invalid_value(L, getglobal_null_name,
			    "lua_getglobal rejects NULL name",
			    "lua_getglobal NULL name error");
  check_fresh_invalid_value(L, settable_invalid_index,
			    "lua_settable rejects invalid table index",
			    "lua_settable invalid table index error");
  check_fresh_invalid_value(L, setfield_invalid_index,
			    "lua_setfield rejects invalid table index",
			    "lua_setfield invalid table index error");
  check_fresh_invalid_value(L, setfield_null_name,
			    "lua_setfield rejects NULL name",
			    "lua_setfield NULL name error");
  check_fresh_invalid_value(L, setglobal_null_name,
			    "lua_setglobal rejects NULL name",
			    "lua_setglobal NULL name error");
  check_fresh_invalid_value(L, newmetatable_null_name,
			    "luaL_newmetatable rejects NULL name",
			    "luaL_newmetatable NULL name error");
  check_fresh_invalid_value(L, getmetafield_null_name,
			    "luaL_getmetafield rejects NULL name",
			    "luaL_getmetafield NULL name error");
  check_fresh_invalid_value(L, callmeta_null_name,
			    "luaL_callmeta rejects NULL name",
			    "luaL_callmeta NULL name error");
  check_fresh_invalid_value(L, callmeta_invalid_index,
			    "luaL_callmeta rejects invalid object index",
			    "luaL_callmeta invalid object index error");
  check_fresh_invalid_value(L, setmetatable_laux_null_name,
			    "luaL_setmetatable rejects NULL name",
			    "luaL_setmetatable NULL name error");
  check_fresh_invalid_value(L, testudata_null_name,
			    "luaL_testudata rejects NULL name",
			    "luaL_testudata NULL name error");
  check_fresh_invalid_value(L, testudata_invalid_index,
			    "luaL_testudata rejects invalid object index",
			    "luaL_testudata invalid object index error");
  check_fresh_invalid_value(L, checkudata_null_name,
			    "luaL_checkudata rejects NULL name",
			    "luaL_checkudata NULL name error");
  check_fresh_invalid_value(L, checkudata_invalid_index,
			    "luaL_checkudata rejects invalid object index",
			    "luaL_checkudata invalid object index error");
  check_fresh_invalid_value(L, getsubtable_null_name,
			    "luaL_getsubtable rejects NULL name",
			    "luaL_getsubtable NULL name error");
  check_fresh_invalid_value(L, requiref_null_name,
			    "luaL_requiref rejects NULL module name",
			    "luaL_requiref NULL module name error");
  check_fresh_invalid_value(L, requiref_null_openf,
			    "luaL_requiref rejects NULL opener",
			    "luaL_requiref NULL opener error");
  check_fresh_invalid_value(L, setmetatable_invalid_object_index,
			    "lua_setmetatable rejects invalid object index",
			    "lua_setmetatable invalid object index error");
  check_fresh_invalid_value(L, getiuservalue_invalid_index,
			    "lua_getiuservalue rejects invalid object index",
			    "lua_getiuservalue invalid object index error");
  check_fresh_invalid_value(L, setiuservalue_invalid_index,
			    "lua_setiuservalue rejects invalid object index",
			    "lua_setiuservalue invalid object index error");

  {
    int top = lua_gettop(L);
    lua_pushinteger(L, 77);
    lua_pushvalue(L, -1);
    check_integer(L, -2, 77, "lua_pushvalue source");
    check_integer(L, -1, 77, "lua_pushvalue copy");
    lua_settop(L, top);
    check(L, lua_gettop(L) == top, "lua_settop restore");
  }

  check(L, luaopen_base_sig(L) == 1, "luaopen_base return");
  check(L, lua_istable(L, -1), "luaopen_base table");
  lua_getfield(L, -1, "assert");
  check(L, lua_isfunction(L, -1), "luaopen_base assert");
  lua_pop(L, 2);
  check(L, lua_gettop(L) == 0, "luaopen_base leaves one public result");

  check(L, luaopen_coroutine_sig(L) == 1, "luaopen_coroutine return");
  check(L, lua_istable(L, -1), "luaopen_coroutine table");
  lua_getfield(L, -1, "create");
  check(L, lua_isfunction(L, -1), "luaopen_coroutine create");
  lua_pop(L, 2);

  lua_pushglobaltable(L);
  check(L, lua_istable(L, -1), "lua_pushglobaltable registry path");
  lua_pushliteral(L, "ok");
  lua_setglobal_sig(L, "__capi_global");
  lua_getglobal_sig(L, "__capi_global");
  check_string(L, -1, "ok", "lua_getglobal after lua_setglobal");
  lua_pop(L, 1);
  lua_pushnil(L);
  lua_setglobal_sig(L, "__capi_global");
  lua_pop(L, 1);

  check(L, LUA_EXTRASPACE == sizeof(void *), "LUA_EXTRASPACE");
  extra = (void **)lua_getextraspace(L);
  *extra = L;
  co = lua_newthread(L);
  check(L, *(void **)lua_getextraspace(co) == L, "lua_getextraspace copy");
  lua_pop(L, 1);

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
  {
    typedef char lua_rawlen_returns_lua_Unsigned[
      _Generic(lua_rawlen(L, -1), lua_Unsigned: 1, default: -1)];
    (void)sizeof(lua_rawlen_returns_lua_Unsigned);
  }
#endif
  lua_pushlstring_sig(L, "rawlen\0hidden", 6);
  check(L, lua_rawlen_sig(L, -1) == 6,
	"lua_rawlen function pointer lua_Unsigned result");
  lua_pop(L, 1);

  check(L, lua_pushstring_sig(L, "pushstring") != NULL,
	"lua_pushstring function pointer return");
  check_string(L, -1, "pushstring", "lua_pushstring function pointer value");
  lua_pop(L, 1);

  check_fresh_invalid_value(L, stringtonumber_null_string,
			    "lua_stringtonumber rejects NULL string",
			    "lua_stringtonumber NULL string error");
  check(L, lua_stringtonumber(L, "123") == 4, "lua_stringtonumber length");
  check_integer(L, -1, 123, "lua_stringtonumber value");
  lua_pop(L, 1);
  if (sizeof(lua_Integer) > sizeof(int)) {
    lua_Integer big40 = (lua_Integer)1024 * 1024 * 1024 * 1024;
    check(L, lua_stringtonumber(L, "1099511627776") == 14,
	  "lua_stringtonumber wider 64-bit length");
    check_integer(L, -1, big40,
		  "lua_stringtonumber wider 64-bit value");
    lua_pop(L, 1);
    check(L, lua_stringtonumber(L, "0x8000000000000000") == 19,
	  "lua_stringtonumber hex mininteger length");
    check(L, lua_isinteger(L, -1),
	  "lua_stringtonumber hex mininteger subtype");
    check_integer(L, -1, LUA_MININTEGER,
		  "lua_stringtonumber hex mininteger value");
    lua_pop(L, 1);
    check(L, lua_stringtonumber(L, "0x10000000000000000") == 20,
	  "lua_stringtonumber hex unsigned wrap length");
    check(L, lua_isinteger(L, -1),
	  "lua_stringtonumber hex unsigned wrap subtype");
    check_integer(L, -1, 0,
		  "lua_stringtonumber hex unsigned wrap value");
    lua_pop(L, 1);
    check(L, lua_stringtonumber(L, "9223372036854775808") == 20,
	  "lua_stringtonumber decimal above max length");
    check(L, !lua_isinteger(L, -1),
	  "lua_stringtonumber decimal above max subtype");
    check(L, lua_tonumber(L, -1) == -(lua_Number)LUA_MININTEGER,
	  "lua_stringtonumber decimal above max value");
    lua_pop(L, 1);
    check(L, lua_stringtonumber(L, "-9223372036854775809") == 21,
	  "lua_stringtonumber decimal below min length");
    check(L, !lua_isinteger(L, -1),
	  "lua_stringtonumber decimal below min subtype");
    check(L, lua_tonumber(L, -1) == (lua_Number)LUA_MININTEGER,
	  "lua_stringtonumber decimal below min value");
    lua_pop(L, 1);
    check(L, lua_stringtonumber(L, "18446744073709551616") == 21,
	  "lua_stringtonumber decimal unsigned range length");
    check(L, !lua_isinteger(L, -1),
	  "lua_stringtonumber decimal unsigned range subtype");
    check(L, lua_tonumber(L, -1) == -(lua_Number)LUA_MININTEGER * 2.0,
	  "lua_stringtonumber decimal unsigned range value");
    lua_pop(L, 1);
  }
  check(L, lua_stringtonumber(L, "nope") == 0, "lua_stringtonumber reject");
  check(L, lua_stringtonumber(L, "inf") == 0, "lua_stringtonumber rejects inf");
  check(L, lua_stringtonumber(L, "NaN") == 0, "lua_stringtonumber rejects nan");
  check(L, lua_stringtonumber(L, "0b10") == 0, "lua_stringtonumber rejects binary prefix");
  lua_pushliteral(L, "inf");
  check(L, !lua_isnumber(L, -1), "lua_isnumber rejects inf string");
  okflag = -1;
  check(L, lua_tonumberx(L, -1, &okflag) == 0 && okflag == 0,
	"lua_tonumberx rejects inf string");
  okflag = -1;
  check(L, lua_tointegerx(L, -1, &okflag) == 0 && okflag == 0,
	"lua_tointegerx rejects inf string");
  lua_pop(L, 1);
  lua_pushliteral(L, "nan");
  check(L, !lua_isnumber(L, -1), "lua_isnumber rejects nan string");
  okflag = -1;
  check(L, lua_tonumberx(L, -1, &okflag) == 0 && okflag == 0,
	"lua_tonumberx rejects nan string");
  okflag = -1;
  check(L, lua_tointegerx(L, -1, &okflag) == 0 && okflag == 0,
	"lua_tointegerx rejects nan string");
  lua_pop(L, 1);
  lua_pushliteral(L, "0b10");
  check(L, !lua_isnumber(L, -1), "lua_isnumber rejects binary prefix string");
  okflag = -1;
  check(L, lua_tonumberx(L, -1, &okflag) == 0 && okflag == 0,
	"lua_tonumberx rejects binary prefix string");
  okflag = -1;
  check(L, lua_tointegerx(L, -1, &okflag) == 0 && okflag == 0,
	"lua_tointegerx rejects binary prefix string");
  lua_pop(L, 1);

  lua_pushnumber(L, (lua_Number)2.5);
  check(L, lua_tonumber(L, -1) == (lua_Number)2.5, "lua_tonumber macro");
  lua_pop(L, 1);

  lua_pushinteger(L, 17);
  check_integer(L, -1, 17, "lua_tointeger macro");
  lua_pop(L, 1);

  ret = lua_pushlstring(L, with_nul, sizeof(with_nul));
  check(L, ret != NULL && memcmp(ret, with_nul, sizeof(with_nul)) == 0,
	"lua_pushlstring return value");
  check(L, lua_rawlen(L, -1) == sizeof(with_nul),
	"lua_pushlstring return length");
  lua_pop(L, 1);

  ret = lua_pushlstring(L, NULL, 0);
  check(L, ret != NULL && lua_rawlen(L, -1) == 0,
	"lua_pushlstring NULL zero length");
  lua_pop(L, 1);

  lua_pushcfunction(L, pushlstring_null_nonzero);
  status = lua_pcall(L, 0, 0, 0);
  check(L, status == LUA_ERRRUN, "lua_pushlstring rejects NULL nonzero");
  check(L, strstr(lua_tostring(L, -1), "invalid value") != NULL,
	"lua_pushlstring NULL nonzero error");
  lua_pop(L, 1);

  ret = lua_pushstring(L, "pushstring-return");
  check(L, ret != NULL && strcmp(ret, "pushstring-return") == 0,
	"lua_pushstring return value");
  lua_pop(L, 1);

  ret = lua_pushstring(L, NULL);
  check(L, ret == NULL && lua_isnil(L, -1), "lua_pushstring NULL return");
  lua_pop(L, 1);

  ret = lua_pushliteral(L, "pushliteral-return");
  check(L, ret != NULL && strcmp(ret, "pushliteral-return") == 0,
	"lua_pushliteral return value");
  lua_pop(L, 1);

  ret = lua_pushfstring(L, "i=%I u=%U f=%f d=%d c=%c s=%s %%",
			(lua_Integer)-123, (long)0x20ac,
			(lua_Number)1.0, 7, 'A', "ok");
  check(L, ret != NULL &&
	   strcmp(ret, "i=-123 u=\xe2\x82\xac f=1.0 d=7 c=A s=ok %") == 0,
	"lua_pushfstring Lua 5.4 formats");
  lua_pop(L, 1);
  {
    const void *ptr = (const void *)0x1234;
    char want[64];
    int wantlen = snprintf(want, sizeof(want), "ptr=%p", ptr);
    check(L, wantlen > 0 && wantlen < (int)sizeof(want),
	  "lua_pushfstring pointer expected string");
    ret = lua_pushfstring(L, "ptr=%p", ptr);
    check(L, ret != NULL && strcmp(ret, want) == 0,
	  "lua_pushfstring uses Lua 5.4 C pointer format");
    lua_pop(L, 1);
  }
  {
    const void *ptr;
    char want[64];
    int wantlen;
    lua_newtable(L);
    ptr = lua_topointer(L, -1);
    wantlen = snprintf(want, sizeof(want), "%p", ptr);
    check(L, wantlen > 0 && wantlen < (int)sizeof(want),
	  "string.format pointer expected string");
    lua_getglobal(L, "string");
    lua_getfield(L, -1, "format");
    lua_pushliteral(L, "%p");
    lua_pushvalue(L, -4);
    lua_call(L, 2, 1);
    check_string(L, -1, want, "string.format uses C pointer format");
    lua_pop(L, 1);
    wantlen = snprintf(want, sizeof(want), "%20p", ptr);
    check(L, wantlen > 0 && wantlen < (int)sizeof(want),
	  "string.format width pointer expected string");
    lua_getfield(L, -1, "format");
    lua_pushliteral(L, "%20p");
    lua_pushvalue(L, -4);
    lua_call(L, 2, 1);
    check_string(L, -1, want, "string.format uses C pointer width");
    lua_pop(L, 1);
    wantlen = snprintf(want, sizeof(want), "%-20p", ptr);
    check(L, wantlen > 0 && wantlen < (int)sizeof(want),
	  "string.format left pointer expected string");
    lua_getfield(L, -1, "format");
    lua_pushliteral(L, "%-20p");
    lua_pushvalue(L, -4);
    lua_call(L, 2, 1);
    check_string(L, -1, want, "string.format uses C pointer left width");
    lua_pop(L, 3);
  }
  if (sizeof(lua_Integer) > sizeof(int)) {
    lua_Integer big40 = (lua_Integer)1024 * 1024 * 1024 * 1024;
    ret = lua_pushfstring(L, "big=%I", big40);
    check(L, ret != NULL && strcmp(ret, "big=1099511627776") == 0,
	  "lua_pushfstring keeps wider 64-bit C integer width");
    lua_pop(L, 1);
    ret = pushvfstring_wrap(L, "vbig=%I", big40);
    check(L, ret != NULL && strcmp(ret, "vbig=1099511627776") == 0,
	  "lua_pushvfstring keeps wider 64-bit C integer width");
    lua_pop(L, 1);
    ret = lua_pushfstring(L, "imax=%I imin=%I",
			  LUA_MAXINTEGER, LUA_MININTEGER);
    check(L, ret != NULL &&
	   strcmp(ret, "imax=9223372036854775807 "
		       "imin=-9223372036854775808") == 0,
	  "lua_pushfstring keeps lua_Integer limits");
    lua_pop(L, 1);
    ret = pushvfstring_wrap(L, "vlim=%I/%I",
			    LUA_MAXINTEGER, LUA_MININTEGER);
    check(L, ret != NULL &&
	   strcmp(ret, "vlim=9223372036854775807/"
		       "-9223372036854775808") == 0,
	  "lua_pushvfstring keeps lua_Integer limits");
    lua_pop(L, 1);
  }

  lua_pushcfunction(L, pushfstring_bad_format);
  check(L, lua_pcall(L, 0, 1, 0) == LUA_ERRRUN,
	"lua_pushfstring invalid format status");
  check(L, strstr(lua_tostring(L, -1),
		  "invalid option '%Z' to 'lua_pushfstring'") != NULL,
	"lua_pushfstring invalid format message");
  lua_pop(L, 1);
  lua_pushcfunction(L, pushfstring_bad_modifier);
  check(L, lua_pcall(L, 0, 1, 0) == LUA_ERRRUN,
	"lua_pushfstring invalid modifier status");
  check(L, strstr(lua_tostring(L, -1),
		  "invalid option '%0' to 'lua_pushfstring'") != NULL,
	"lua_pushfstring invalid modifier message");
  lua_pop(L, 1);
  lua_pushcfunction(L, pushfstring_bad_unsigned);
  check(L, lua_pcall(L, 0, 1, 0) == LUA_ERRRUN,
	"lua_pushfstring invalid unsigned status");
  check(L, strstr(lua_tostring(L, -1),
		  "invalid option '%u' to 'lua_pushfstring'") != NULL,
	"lua_pushfstring invalid unsigned message");
  lua_pop(L, 1);
  lua_pushcfunction(L, pushfstring_bad_floatfmt);
  check(L, lua_pcall(L, 0, 1, 0) == LUA_ERRRUN,
	"lua_pushfstring invalid float format status");
  check(L, strstr(lua_tostring(L, -1),
		  "invalid option '%g' to 'lua_pushfstring'") != NULL,
	"lua_pushfstring invalid float format message");
  lua_pop(L, 1);
  lua_pushcfunction(L, pushfstring_null_format);
  status = lua_pcall(L, 0, 0, 0);
  check(L, status == LUA_ERRRUN, "lua_pushfstring rejects NULL format");
  check(L, strstr(lua_tostring(L, -1), "invalid value") != NULL,
	"lua_pushfstring NULL format error");
  lua_pop(L, 1);
  lua_pushcfunction(L, pushvfstring_null_format);
  status = lua_pcall(L, 0, 0, 0);
  check(L, status == LUA_ERRRUN, "lua_pushvfstring rejects NULL format");
  check(L, strstr(lua_tostring(L, -1), "invalid value") != NULL,
	"lua_pushvfstring NULL format error");
  lua_pop(L, 1);

  lua_pushliteral(L, "Lua");
  lua_pushliteral(L, "5");
  lua_pushliteral(L, ".");
  lua_pushliteral(L, "4");
  lua_concat(L, 4);
  check_string(L, -1, "Lua5.4", "lua_concat strings");
  lua_pop(L, 1);
  lua_concat(L, 0);
  check_string(L, -1, "", "lua_concat zero values");
  lua_pop(L, 1);

  check(L, lua_numbertointeger((lua_Number)42, &iv) && iv == 42,
	"lua_numbertointeger integer");
  iv = 0;
  check(L, lua_numbertointeger((lua_Number)1.5, &iv) && iv == 1,
	"Lua 5.4 lua_numbertointeger truncates in-range fractions");
  iv = 0;
  if (sizeof(lua_Integer) > sizeof(int)) {
    lua_Integer big40 = (lua_Integer)1024 * 1024 * 1024 * 1024;
    check(L, LUA_MAXINTEGER > (lua_Integer)0x7fffffffu,
	  "LUA_MAXINTEGER follows external lua_Integer width");
    check(L, LUA_MININTEGER < (lua_Integer)(-2147483647 - 1),
	  "LUA_MININTEGER follows external lua_Integer width");
    check(L, lua_numbertointeger((lua_Number)2147483648.0, &iv) &&
	     iv == (lua_Integer)((lua_Unsigned)0x7fffffffu + 1u),
	  "lua_numbertointeger accepts external 64-bit header range");
    iv = 0;
    check(L, lua_numbertointeger((lua_Number)big40, &iv) && iv == big40,
	  "lua_numbertointeger accepts wider exact 64-bit header range");
    iv = 0;
    check(L, lua_numbertointeger((lua_Number)LUA_MININTEGER, &iv) &&
	     iv == LUA_MININTEGER,
	  "lua_numbertointeger accepts LUA_MININTEGER bound");
    iv = 0;
  }
  check(L, !lua_numbertointeger((lua_Number)LUA_MAXINTEGER + 1.0, &iv),
	"lua_numbertointeger rejects upper exclusive bound");
  {
    lua_Number pinf = (lua_Number)HUGE_VAL;
    lua_Number nanv = pinf - pinf;
    check(L, !lua_numbertointeger(pinf, &iv),
	  "lua_numbertointeger rejects positive infinity");
    check(L, !lua_numbertointeger(-pinf, &iv),
	  "lua_numbertointeger rejects negative infinity");
    check(L, !lua_numbertointeger(nanv, &iv),
	  "lua_numbertointeger rejects NaN");
  }
  {
    char nbuf[64];
    char ibuf[64];
    lua_number2str(nbuf, sizeof(nbuf), (lua_Number)12.5);
    lua_integer2str(ibuf, sizeof(ibuf), (lua_Integer)-123);
    check(L, strcmp(nbuf, "12.5") == 0, "lua_number2str 5.4 signature");
    check(L, strcmp(ibuf, "-123") == 0, "lua_integer2str");
    if (sizeof(lua_Integer) > sizeof(int)) {
      lua_Integer big40 = (lua_Integer)1024 * 1024 * 1024 * 1024;
      lua_integer2str(ibuf, sizeof(ibuf),
		      (lua_Integer)((lua_Unsigned)0x7fffffffu + 1u));
      check(L, strcmp(ibuf, "2147483648") == 0,
	    "lua_integer2str keeps 64-bit C integer width");
      lua_integer2str(ibuf, sizeof(ibuf), big40);
      check(L, strcmp(ibuf, "1099511627776") == 0,
	    "lua_integer2str keeps wider 64-bit C integer width");
      lua_integer2str(ibuf, sizeof(ibuf), LUA_MAXINTEGER);
      check(L, strcmp(ibuf, "9223372036854775807") == 0,
	    "lua_integer2str keeps LUA_MAXINTEGER");
      lua_integer2str(ibuf, sizeof(ibuf), LUA_MININTEGER);
      check(L, strcmp(ibuf, "-9223372036854775808") == 0,
	    "lua_integer2str keeps LUA_MININTEGER");
    }
    check(L, LUA_MAXUNSIGNED == (lua_Unsigned)~(lua_Unsigned)0,
	  "LUA_MAXUNSIGNED");
    if (sizeof(lua_Unsigned) > sizeof(unsigned int))
      check(L, LUA_MAXUNSIGNED > (lua_Unsigned)0xffffffffu,
	    "lua_Unsigned is not capped to unsigned int");
  }
  lua_pushnumber(L, (lua_Number)1.5);
  check(L, lua_tointegerx(L, -1, NULL) == 0,
	"lua_tointegerx fraction value");
  {
    int ok = 1;
    lua_tointegerx(L, -1, &ok);
    check(L, !ok, "lua_tointegerx fraction status");
  }
  lua_pop(L, 1);

  if (sizeof(lua_Integer) > sizeof(int)) {
    lua_Integer big = (lua_Integer)((lua_Unsigned)0x7fffffffu + 1u);
    lua_Integer big40 = (lua_Integer)1024 * 1024 * 1024 * 1024;
    lua_Number pos63 = -(lua_Number)LUA_MININTEGER;
    int ok = 0;
    lua_pushinteger(L, big);
    check(L, lua_tointegerx(L, -1, &ok) == big && ok,
	  "lua_tointegerx accepts pushed 64-bit C integer");
    lua_pop(L, 1);
    lua_pushnumber(L, (lua_Number)2147483648.0);
    check(L, lua_tointegerx(L, -1, &ok) == big && ok,
	  "lua_tointegerx accepts exact 64-bit number");
    lua_pop(L, 1);
    lua_pushliteral(L, "2147483648");
    check(L, lua_tointegerx(L, -1, &ok) == big && ok,
	  "lua_tointegerx accepts exact 64-bit string integer");
    lua_pop(L, 1);
    lua_pushinteger(L, big40);
    check(L, lua_tointegerx(L, -1, &ok) == big40 && ok,
	  "lua_tointegerx accepts pushed wider 64-bit C integer");
    lua_pop(L, 1);
    lua_pushinteger(L, big40);
    check(L, lua_tonumberx(L, -1, &ok) == (lua_Number)big40 && ok,
	  "lua_tonumberx accepts boxed wider 64-bit C integer");
    lua_pop(L, 1);
    lua_pushnumber(L, (lua_Number)big40);
    check(L, lua_tointegerx(L, -1, &ok) == big40 && ok,
	  "lua_tointegerx accepts wider exact 64-bit number");
    lua_pop(L, 1);
    lua_pushliteral(L, "1099511627776");
    check(L, lua_tointegerx(L, -1, &ok) == big40 && ok,
	  "lua_tointegerx accepts wider exact 64-bit string integer");
    lua_pop(L, 1);
    lua_pushliteral(L, "1099511627776");
    check(L, lua_tonumberx(L, -1, &ok) == (lua_Number)big40 && ok,
	  "lua_tonumberx accepts wider exact 64-bit string number");
    lua_pop(L, 1);
    lua_pushliteral(L, "9223372036854775808");
    check(L, lua_tonumberx(L, -1, &ok) == pos63 && ok,
	  "lua_tonumberx accepts decimal above LUA_MAXINTEGER as number");
    lua_pop(L, 1);
    lua_pushliteral(L, "-9223372036854775809");
    check(L, lua_tonumberx(L, -1, &ok) == -pos63 && ok,
	  "lua_tonumberx accepts decimal below LUA_MININTEGER as number");
    lua_pop(L, 1);
    lua_pushliteral(L, "18446744073709551616");
    check(L, lua_tonumberx(L, -1, &ok) == pos63 * 2.0 && ok,
	  "lua_tonumberx accepts decimal unsigned range as number");
    lua_pop(L, 1);
    lua_pushinteger(L, LUA_MAXINTEGER);
    check(L, lua_isinteger(L, -1), "lua_isinteger accepts LUA_MAXINTEGER");
    check(L, lua_tointegerx(L, -1, &ok) == LUA_MAXINTEGER && ok,
	  "lua_tointegerx accepts pushed LUA_MAXINTEGER");
    lua_pop(L, 1);
    lua_pushinteger(L, LUA_MININTEGER);
    check(L, lua_isinteger(L, -1), "lua_isinteger accepts LUA_MININTEGER");
    check(L, lua_tointegerx(L, -1, &ok) == LUA_MININTEGER && ok,
	  "lua_tointegerx accepts pushed LUA_MININTEGER");
    lua_pop(L, 1);
    lua_pushliteral(L, "9223372036854775807");
    check(L, lua_tointegerx(L, -1, &ok) == LUA_MAXINTEGER && ok,
	  "lua_tointegerx accepts string LUA_MAXINTEGER");
    lua_pop(L, 1);
    lua_pushliteral(L, "-9223372036854775808");
    check(L, lua_tointegerx(L, -1, &ok) == LUA_MININTEGER && ok,
	  "lua_tointegerx accepts string LUA_MININTEGER");
    lua_pop(L, 1);
    lua_pushliteral(L, "9223372036854775808");
    check(L, lua_tointegerx(L, -1, &ok) == 0 && !ok,
	  "lua_tointegerx rejects string above LUA_MAXINTEGER");
    lua_pop(L, 1);
    lua_pushliteral(L, "-9223372036854775809");
    check(L, lua_tointegerx(L, -1, &ok) == LUA_MININTEGER && ok,
	  "lua_tointegerx accepts rounded string below LUA_MININTEGER");
    lua_pop(L, 1);
    lua_pushliteral(L, "0x8000000000000000");
    check(L, lua_tointegerx(L, -1, &ok) == LUA_MININTEGER && ok,
	  "lua_tointegerx accepts hex LUA_MININTEGER");
    lua_pop(L, 1);
    lua_pushliteral(L, "0xffffffffffffffff");
    check(L, lua_tointegerx(L, -1, &ok) == (lua_Integer)-1 && ok,
	  "lua_tointegerx accepts hex unsigned minus one");
    lua_pop(L, 1);
    lua_pushliteral(L, "0x10000000000000000");
    check(L, lua_tointegerx(L, -1, &ok) == 0 && ok,
	  "lua_tointegerx accepts hex unsigned wrap");
    lua_pop(L, 1);
  }

  lua_newtable(L);
  lua_pushinteger(L, 11);
  lua_setfield(L, -2, "a");
  lua_pushinteger(L, 22);
  lua_setfield(L, -2, "b");
  {
    lua_Integer sum = 0;
    int count = 0;
    lua_pushnil(L);
    while (lua_next(L, -2) != 0) {
      int ok = 0;
      sum += lua_tointegerx(L, -1, &ok);
      check(L, ok, "lua_next value integer");
      count++;
      lua_pop(L, 1);
    }
    check(L, count == 2 && sum == 33, "lua_next table traversal");
  }
  lua_pop(L, 1);

  if (sizeof(lua_Integer) > sizeof(int)) {
    lua_Integer big40 = (lua_Integer)1024 * 1024 * 1024 * 1024;
    lua_Integer neg40 = -big40;
    int count = 0;
    int seen_pos = 0;
    int seen_neg = 0;
    lua_newtable(L);
    lua_pushinteger(L, big40);
    lua_pushliteral(L, "next-big40");
    lua_rawset(L, -3);
    lua_pushinteger(L, neg40);
    lua_pushliteral(L, "next-neg40");
    lua_rawset(L, -3);
    lua_pushnil(L);
    while (lua_next(L, -2) != 0) {
      lua_Integer key;
      check(L, lua_isinteger(L, -2),
	    "lua_next preserves 64-bit integer key subtype");
      key = lua_tointeger(L, -2);
      if (key == big40) {
	check_string(L, -1, "next-big40",
		     "lua_next positive 64-bit key value");
	seen_pos = 1;
      } else if (key == neg40) {
	check_string(L, -1, "next-neg40",
		     "lua_next negative 64-bit key value");
	seen_neg = 1;
      } else {
	check(L, 0, "lua_next unexpected 64-bit key");
      }
      count++;
      lua_pop(L, 1);
    }
    check(L, count == 2 && seen_pos && seen_neg,
	  "lua_next traverses signed 64-bit integer keys");
    lua_pop(L, 1);
  }

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

  lua_pushcfunction(L, concat_negative_count);
  status = lua_pcall(L, 0, 0, 0);
  check(L, status == LUA_ERRRUN, "lua_concat rejects negative count");
  check(L, strstr(lua_tostring(L, -1), "invalid value") != NULL,
	"lua_concat negative count error");
  lua_pop(L, 1);

  lua_pushcfunction(L, concat_large_count);
  status = lua_pcall(L, 0, 0, 0);
  check(L, status == LUA_ERRRUN, "lua_concat rejects large count");
  check(L, strstr(lua_tostring(L, -1), "invalid value") != NULL,
	"lua_concat large count error");
  lua_pop(L, 1);

  lua_pushliteral(L, "one");
  lua_pushliteral(L, "two");
  lua_pushliteral(L, "three");
  lua_insert(L, -3);
  check_string(L, -3, "three", "lua_insert macro first");
  check_string(L, -2, "one", "lua_insert macro second");
  check_string(L, -1, "two", "lua_insert macro third");
  lua_remove(L, -2);
  check_string(L, -2, "three", "lua_remove macro first");
  check_string(L, -1, "two", "lua_remove macro second");
  lua_pushliteral(L, "replacement");
  lua_replace(L, -2);
  check_string(L, -2, "three", "lua_replace macro first");
  check_string(L, -1, "replacement", "lua_replace macro target");
  lua_pop(L, 2);

  lua_pushliteral(L, "copy-source");
  lua_pushnil(L);
  lua_copy(L, -2, -1);
  check_string(L, -1, "copy-source", "lua_copy destination");
  lua_pop(L, 2);

  check(L, lua_isyieldable(L) == 0, "lua_isyieldable main C frame");

  lua_pushcfunction(L, push_answer);
  lua_call(L, 0, 1);
  check_integer(L, -1, 42, "lua_call macro");
  lua_pop(L, 1);

  check_fresh_invalid_value(L, call_missing_function,
			    "lua_call rejects missing function",
			    "lua_call missing function error");
  check_fresh_invalid_value(L, call_negative_nargs,
			    "lua_call rejects negative nargs",
			    "lua_call negative nargs error");
  check_fresh_invalid_value(L, call_invalid_nresults,
			    "lua_call rejects invalid nresults",
			    "lua_call invalid nresults error");

  lua_pushcfunction(L, push_answer);
  check(L, lua_pcall(L, 0, 1, 0) == LUA_OK, "lua_pcall macro");
  check_integer(L, -1, 42, "lua_pcall macro result");
  lua_pop(L, 1);

  check_fresh_invalid_value(L, pcall_missing_function,
			    "lua_pcall rejects missing function",
			    "lua_pcall missing function error");
  check_fresh_invalid_value(L, pcall_negative_nargs,
			    "lua_pcall rejects negative nargs",
			    "lua_pcall negative nargs error");
  check_fresh_invalid_value(L, pcall_invalid_nresults,
			    "lua_pcall rejects invalid nresults",
			    "lua_pcall invalid nresults error");

  lua_pushcfunction(L, push_answer);
  lua_callk(L, 0, 1, 0, NULL);
  check_integer(L, -1, 42, "lua_callk macro");
  lua_pop(L, 1);

  lua_pushcfunction(L, push_answer);
  lua_callk_sig(L, 0, 1, 0, NULL);
  check_integer(L, -1, 42, "lua_callk function pointer");
  lua_pop(L, 1);

  capi_unexpected_cont_called = 0;
  lua_pushcfunction(L, push_answer);
  lua_callk_sig(L, 0, 1, (lua_KContext)0x6400, unexpected_capi_cont);
  check_integer(L, -1, 42, "lua_callk non-yielding continuation result");
  check(L, capi_unexpected_cont_called == 0,
	"lua_callk non-yielding continuation not called");
  lua_pop(L, 1);

  lua_pushcfunction(L, push_answer);
  check(L, lua_pcallk(L, 0, 1, 0, 0, NULL) == LUA_OK, "lua_pcallk macro");
  check_integer(L, -1, 42, "lua_pcallk result");
  lua_pop(L, 1);

  lua_pushcfunction(L, push_answer);
  check(L, lua_pcallk_sig(L, 0, 1, 0, 0, NULL) == LUA_OK,
	"lua_pcallk function pointer");
  check_integer(L, -1, 42, "lua_pcallk function pointer result");
  lua_pop(L, 1);

  capi_unexpected_cont_called = 0;
  lua_pushcfunction(L, push_answer);
  check(L, lua_pcallk_sig(L, 0, 1, 0, (lua_KContext)0x6500,
			  unexpected_capi_cont) == LUA_OK,
	"lua_pcallk non-yielding continuation status");
  check_integer(L, -1, 42, "lua_pcallk non-yielding continuation result");
  check(L, capi_unexpected_cont_called == 0,
	"lua_pcallk non-yielding continuation not called");
  lua_pop(L, 1);

  capi_unexpected_cont_called = 0;
  lua_pushcfunction(L, raise_lua_error);
  check(L, lua_pcallk_sig(L, 0, 0, 0, (lua_KContext)0x6501,
			  unexpected_capi_cont) == LUA_ERRRUN,
	"lua_pcallk non-yielding error continuation status");
  check_string(L, -1, "capi raised error",
	"lua_pcallk non-yielding error object");
  check(L, capi_unexpected_cont_called == 0,
	"lua_pcallk non-yielding error continuation not called");
  lua_pop(L, 1);

  lua_pushcfunction(L, raise_lua_error);
  check(L, lua_pcall(L, 0, 0, 0) == LUA_ERRRUN, "lua_error status");
  check_string(L, -1, "capi raised error", "lua_error message");
  lua_pop(L, 1);
  check_fresh_invalid_value(L, error_missing_object,
			    "lua_error rejects missing error object",
			    "lua_error missing object error");

  close_call_count = 0;
  close_nil_error_count = 0;
  push_closeable(L, record_close);
  lua_toclose(L, -1);
  lua_closeslot(L, -1);
  check(L, lua_isnil(L, -1), "lua_closeslot nils closed slot");
  check(L, close_call_count == 1 && close_nil_error_count == 1,
	"lua_closeslot calls __close with nil error");
  lua_pop(L, 1);

  lua_pushcfunction(L, mark_false_then_closeslot);
  check(L, lua_pcall(L, 0, 0, 0) == LUA_ERRRUN,
	"lua_closeslot rejects ignored false slot");
  check(L, strstr(lua_tostring(L, -1), "no variable to close") != NULL,
	"lua_closeslot ignored false error text");
  lua_pop(L, 1);

  lua_pushcfunction(L, mark_nonclosable_slot);
  check(L, lua_pcall(L, 0, 0, 0) == LUA_ERRRUN,
	"lua_toclose rejects non-closable values");
  check(L, strstr(lua_tostring(L, -1), "non-closable") != NULL,
	"lua_toclose non-closable error text");
  lua_pop(L, 1);

  lua_pushcfunction(L, mark_close_twice_same_slot);
  check(L, lua_pcall(L, 0, 0, 0) == LUA_ERRRUN,
	"lua_toclose rejects same marked slot");
  check(L, strstr(lua_tostring(L, -1), "below or equal") != NULL,
	"lua_toclose same slot error text");
  lua_pop(L, 1);

  lua_pushcfunction(L, mark_close_below_active_slot);
  check(L, lua_pcall(L, 0, 0, 0) == LUA_ERRRUN,
	"lua_toclose rejects below active slot");
  check(L, strstr(lua_tostring(L, -1), "below or equal") != NULL,
	"lua_toclose below active error text");
  lua_pop(L, 1);

  lua_pushcfunction(L, mark_nil_below_active_slot);
  check(L, lua_pcall(L, 0, 0, 0) == LUA_ERRRUN,
	"lua_toclose rejects nil below active slot");
  check(L, strstr(lua_tostring(L, -1), "below or equal") != NULL,
	"lua_toclose nil below active error text");
  lua_pop(L, 1);

  lua_pushcfunction(L, closeslot_not_last);
  check(L, lua_pcall(L, 0, 0, 0) == LUA_ERRRUN,
	"lua_closeslot rejects non-last slot");
  check(L, strstr(lua_tostring(L, -1), "no variable to close") != NULL,
	"lua_closeslot non-last error text");
  lua_pop(L, 1);

  close_call_count = 0;
  close_nil_error_count = 0;
  lua_pushcfunction(L, closeslot_lifo);
  check(L, lua_pcall(L, 0, 0, 0) == LUA_OK,
	"lua_closeslot accepts last slots in LIFO order");
  check(L, close_call_count == 2 && close_nil_error_count == 2,
	"lua_closeslot LIFO closes both slots");

  close_call_count = 0;
  close_nil_error_count = 0;
  lua_pushcfunction(L, mark_close_then_pop);
  check(L, lua_pcall(L, 0, 0, 0) == LUA_OK,
	"lua_settop closes toclose slot");
  check(L, close_call_count == 1 && close_nil_error_count == 1,
	"lua_settop close uses nil error");

  close_call_count = 0;
  close_nil_error_count = 0;
  lua_pushcfunction(L, mark_close_then_return);
  check(L, lua_pcall(L, 0, 0, 0) == LUA_OK,
	"C return closes toclose slot");
  check(L, close_call_count == 1 && close_nil_error_count == 1,
	"C return close uses nil error");

  close_call_count = 0;
  close_nil_error_count = 0;
  lua_pushcfunction(L, mark_close_then_return_self);
  check(L, lua_pcall(L, 0, 1, 0) == LUA_OK,
	"C return keeps closed result slot");
  check(L, lua_istable(L, -1), "C return keeps closed table result");
  check(L, close_call_count == 1 && close_nil_error_count == 1,
	"C return closes returned slot with nil error");
  lua_pop(L, 1);

  co = lua_newthread(L);
  lua_pushcfunction(co, mark_lua_close_yield_then_return);
  {
    int nres = -1;
    check(L, lua_resume_sig(co, L, 0, &nres) == LUA_YIELD,
	  "C return close-yield initial status");
    check(L, nres == 1 && lua_gettop(co) == 1,
	  "C return close-yield yield count");
    check_string(co, 1, "capi close yield",
	  "C return close-yield value");
    lua_settop(co, 0);
    nres = -1;
    check(L, lua_resume_sig(co, L, 0, &nres) == LUA_OK,
	  "C return close-yield final status");
    check(L, nres == 1 && lua_gettop(co) == 1,
	  "C return close-yield final count");
    check_string(co, 1, "capi return after close yield",
	  "C return close-yield final result");
  }
  lua_pop(L, 1);

  co = lua_newthread(L);
  lua_pushcfunction(co, mark_two_lua_close_yield_then_return);
  {
    int nres = -1;
    int status = lua_resume_sig(co, L, 0, &nres);
    check(L, status == LUA_YIELD,
	  "C return two close-yields first status");
    check(L, nres == 1 && lua_gettop(co) == 1,
	  "C return two close-yields first count");
    check_string(co, 1, "capi inner close yield",
	  "C return two close-yields first value");
    lua_settop(co, 0);
    nres = -1;
    status = lua_resume_sig(co, L, 0, &nres);
    check(L, status == LUA_YIELD,
	  "C return two close-yields second status");
    check(L, nres == 1 && lua_gettop(co) == 1,
	  "C return two close-yields second count");
    check_string(co, 1, "capi outer close yield",
	  "C return two close-yields second value");
    lua_settop(co, 0);
    nres = -1;
    status = lua_resume_sig(co, L, 0, &nres);
    check(L, status == LUA_OK,
	  "C return two close-yields final status");
    check(L, nres == 1 && lua_gettop(co) == 1,
	  "C return two close-yields final count");
    check_string(co, 1, "capi return after two close yields",
	  "C return two close-yields final result");
  }
  lua_pop(L, 1);

  co = lua_newthread(L);
  lua_pushcfunction(co, mark_lua_close_yield_then_return_self);
  {
    int nres = -1;
    check(L, lua_resume_sig(co, L, 0, &nres) == LUA_YIELD,
	  "C return yielded close self initial status");
    check(L, nres == 1 && lua_gettop(co) == 1,
	  "C return yielded close self initial count");
    check_string(co, 1, "capi returned close yield",
	  "C return yielded close self value");
    lua_settop(co, 0);
    nres = -1;
    check(L, lua_resume_sig(co, L, 0, &nres) == LUA_OK,
	  "C return yielded close self final status");
    check(L, nres == 1 && lua_gettop(co) == 1 && lua_istable(co, 1),
	  "C return yielded close self final table");
  }
  lua_pop(L, 1);

  co = lua_newthread(L);
  lua_pushcfunction(co, mark_lua_close_yield_error_then_return);
  {
    int nres = -1;
    check(L, lua_resume_sig(co, L, 0, &nres) == LUA_YIELD,
	  "C return close-yield-error initial status");
    check(L, nres == 1 && lua_gettop(co) == 1,
	  "C return close-yield-error yield count");
    check_string(co, 1, "capi close yield before error",
	  "C return close-yield-error value");
    lua_settop(co, 0);
    nres = -1;
    check(L, lua_resume_sig(co, L, 0, &nres) == LUA_ERRRUN,
	  "C return close-yield-error final status");
    check(L, nres == 1 && lua_gettop(co) == 1,
	  "C return close-yield-error final count");
    check_string(co, 1, "capi close yield boom",
	  "C return close-yield-error object");
  }
  lua_pop(L, 1);

  close_call_count = 0;
  close_body_error_count = 0;
  co = lua_newthread(L);
  lua_pushcfunction(co, mark_lua_close_yield_error_with_outer_close);
  {
    int nres = -1;
    check(L, lua_resume_sig(co, L, 0, &nres) == LUA_YIELD,
	  "C return close-yield-error outer initial status");
    check(L, nres == 1 && lua_gettop(co) == 1,
	  "C return close-yield-error outer initial count");
    check_string(co, 1, "capi inner close yield before error",
	  "C return close-yield-error outer value");
    lua_settop(co, 0);
    nres = -1;
    check(L, lua_resume_sig(co, L, 0, &nres) == LUA_ERRRUN,
	  "C return close-yield-error outer final status");
    check(L, nres == 1 && lua_gettop(co) == 1,
	  "C return close-yield-error outer final count");
    check_string(co, 1, "capi inner close boom",
	  "C return close-yield-error outer object");
    check(L, close_call_count == 1 && close_body_error_count == 1,
	  "C return close-yield-error outer receives replacement error");
  }
  lua_pop(L, 1);

  close_call_count = 0;
  close_nil_error_count = 0;
  lua_pushcfunction(L, mark_close_error_then_return);
  check(L, lua_pcall(L, 0, 0, 0) == LUA_ERRRUN,
	"C return close error status");
  check(L, close_call_count == 1 && close_nil_error_count == 1,
	"C return close error uses nil original error");
  check(L, strstr(lua_tostring(L, -1), "capi close boom") != NULL,
	"C return close error text");
  lua_pop(L, 1);

  close_call_count = 0;
  close_body_error_count = 0;
  lua_pushcfunction(L, mark_close_then_error);
  check(L, lua_pcall(L, 0, 0, 0) == LUA_ERRRUN,
	"lua error closes toclose slot");
  check(L, close_call_count == 1 && close_body_error_count == 1,
	"lua error close receives body error");
  check(L, strstr(lua_tostring(L, -1), "capi body boom") != NULL,
	"lua error keeps body error");
  lua_pop(L, 1);

  close_call_count = 0;
  close_nil_error_count = 0;
  lua_pushcfunction(L, push_closeable_userdata);
  lua_setglobal(L, "capi_closeable_userdata");
  check(L, luaL_loadstring(L,
	"local x <close> = capi_closeable_userdata(); return true") == LUA_OK,
	"full userdata tbc load");
  check(L, lua_pcall(L, 0, 1, 0) == LUA_OK,
	"full userdata tbc scope exit");
  check(L, lua_toboolean(L, -1) == 1, "full userdata tbc result");
  check(L, close_call_count == 1 && close_nil_error_count == 1,
	"full userdata tbc closes with nil error");
  lua_pop(L, 1);

  close_call_count = 0;
  close_nil_error_count = 0;
  lua_pushcfunction(L, mark_userdata_close_then_return);
  check(L, lua_pcall(L, 0, 0, 0) == LUA_OK,
	"C return closes toclose userdata slot");
  check(L, close_call_count == 1 && close_nil_error_count == 1,
	"C return userdata close uses nil error");

  co = lua_newthread(L);
  check(L, lua_pushthread(co) == 0, "lua_pushthread coroutine return");
  check(L, lua_tothread(co, -1) == co, "lua_pushthread coroutine value");
  lua_pop(co, 1);
  lua_pop(L, 1);

  co = lua_newthread(L);
  lua_pushcfunction(L, yield_once);
  lua_xmove(L, co, 1);
  check(L, lua_resume(co, L, 0, NULL) == LUA_YIELD,
	"lua_resetthread setup yield");
  check(L, lua_status(co) == LUA_YIELD, "lua_resetthread yielded status");
  check(L, lua_resetthread(co) == LUA_OK, "lua_resetthread return");
  check(L, lua_status(co) == LUA_OK, "lua_resetthread status");
  check(L, lua_gettop(co) == 0, "lua_resetthread clears stack");
  lua_pop(L, 1);

  co = lua_newthread(L);
  lua_pushcfunction(L, yield_once);
  lua_xmove(L, co, 1);
  check(L, lua_resume(co, L, 0, NULL) == LUA_YIELD,
	"lua_closethread setup yield");
  check(L, lua_closethread(co, L) == LUA_OK,
	"lua_closethread yielded return");
  check(L, lua_status(co) == LUA_OK, "lua_closethread yielded status");
  check(L, lua_gettop(co) == 0, "lua_closethread yielded clears stack");
  lua_pop(L, 1);

  lua_pushcfunction(L, record_close);
  lua_setglobal(L, "capi_record_close");
  co = lua_newthread(L);
  close_call_count = 0;
  close_nil_error_count = 0;
  check(L, luaL_loadstring(co,
	"local mt={__close=capi_record_close}; "
	"local x <close> = setmetatable({}, mt); coroutine.yield('paused')") ==
	LUA_OK, "lua_closethread tbc load");
  check(L, lua_resume(co, L, 0, NULL) == LUA_YIELD,
	"lua_closethread tbc yield");
  check(L, lua_closethread(co, L) == LUA_OK,
	"lua_closethread tbc return");
  check(L, close_call_count == 1 && close_nil_error_count == 1,
	"lua_closethread closes tbc with nil error");
  check(L, lua_status(co) == LUA_OK, "lua_closethread tbc status");
  check(L, lua_gettop(co) == 0, "lua_closethread tbc clears stack");
  lua_pop(L, 1);

  lua_pushcfunction(L, record_close_error);
  lua_setglobal(L, "capi_record_close_error");
  co = lua_newthread(L);
  close_call_count = 0;
  close_nil_error_count = 0;
  check(L, luaL_loadstring(co,
	"local mt={__close=capi_record_close_error}; "
	"local x <close> = setmetatable({}, mt); coroutine.yield('paused')") ==
	LUA_OK, "lua_closethread tbc close-error load");
  check(L, lua_resume(co, L, 0, NULL) == LUA_YIELD,
	"lua_closethread tbc close-error yield");
  check(L, lua_closethread(co, L) == LUA_ERRRUN,
	"lua_closethread tbc close-error return");
  check_string(co, 1, "capi close boom",
	"lua_closethread tbc close-error object");
  check(L, close_call_count == 1 && close_nil_error_count == 1,
	"lua_closethread reports close error after nil close");
  lua_pop(L, 1);

  co = lua_newthread(L);
  check(L, luaL_loadstring(co,
	"local mt={__close=function() coroutine.yield('closing') end}; "
	"local x <close> = setmetatable({}, mt); coroutine.yield('paused')") ==
	LUA_OK, "lua_closethread tbc close-yield load");
  check(L, lua_resume(co, L, 0, NULL) == LUA_YIELD,
	"lua_closethread tbc close-yield pause");
  /* lua_closethread() is a plain C API call, so a __close metamethod that
  ** tries to yield must become the final close error and leave no resumable
  ** half-closed coroutine state behind.
  */
  check(L, lua_closethread(co, L) == LUA_ERRRUN,
	"lua_closethread tbc close-yield return");
  check(L, strstr(lua_tostring(co, 1), "yield across") != NULL,
	"lua_closethread tbc close-yield error");
  check(L, lua_status(co) == LUA_OK, "lua_closethread tbc close-yield status");
  lua_settop(co, 0);
  check(L, lua_closethread(co, L) == LUA_OK,
	"lua_closethread tbc close-yield second close");
  lua_pop(L, 1);

  co = lua_newthread(L);
  check(L, luaL_loadstring(co,
	"local mt={__close=function() coroutine.yield('closing') end}; "
	"local x <close> = setmetatable({}, mt); coroutine.yield('paused')") ==
	LUA_OK, "lua_resetthread tbc close-yield load");
  check(L, lua_resume(co, L, 0, NULL) == LUA_YIELD,
	"lua_resetthread tbc close-yield pause");
  check(L, lua_resetthread(co) == LUA_ERRRUN,
	"lua_resetthread tbc close-yield return");
  check(L, strstr(lua_tostring(co, 1), "yield across") != NULL,
	"lua_resetthread tbc close-yield error");
  check(L, lua_status(co) == LUA_OK, "lua_resetthread tbc close-yield status");
  lua_settop(co, 0);
  check(L, lua_resetthread(co) == LUA_OK,
	"lua_resetthread tbc close-yield second reset");
  lua_pop(L, 1);

  co = lua_newthread(L);
  check(L, lua_closethread(co, L) == LUA_OK,
	"lua_closethread fresh return");
  check(L, lua_status(co) == LUA_OK, "lua_closethread fresh status");
  check(L, lua_gettop(co) == 0, "lua_closethread fresh stack");
  lua_pop(L, 1);

  co = lua_newthread(L);
  lua_pushcfunction(L, push_isyieldable);
  lua_xmove(L, co, 1);
  {
    int nres = -1;
    check(L, lua_resume(co, L, 0, &nres) == LUA_OK,
	  "lua_isyieldable coroutine resume status");
    check(L, nres == 1 && lua_gettop(co) == 1,
	  "lua_isyieldable coroutine result count");
    check(L, lua_toboolean(co, 1) == 1,
	  "lua_isyieldable resumed C frame");
  }
  lua_pop(L, 1);

  check_resume_invalid_value(L, yield_negative_results,
			     "lua_yield rejects negative nresults",
			     "lua_yield negative nresults error");
  check_resume_invalid_value(L, yield_too_many_results,
			     "lua_yield rejects too many results",
			     "lua_yield too many results error");
  check_resume_invalid_value(L, yieldk_negative_results,
			     "lua_yieldk rejects negative nresults",
			     "lua_yieldk negative nresults error");
  check_yieldk_cont_invalid_value(L, yield_with_negative_cont,
				  "lua_yieldk negative continuation initial yield",
				  "lua_yieldk rejects negative continuation result count",
				  "lua_yieldk negative continuation result error");
  check_yieldk_cont_invalid_value(L, yield_with_too_many_cont,
				  "lua_yieldk large continuation initial yield",
				  "lua_yieldk rejects too-large continuation result count",
				  "lua_yieldk too-large continuation result error");

  co = lua_newthread(L);
  lua_pushcfunction(L, yield_two);
  lua_xmove(L, co, 1);
  {
    int nres = -1;
    check(L, lua_resume_sig(co, L, 0, &nres) == LUA_YIELD,
	  "lua_resume function pointer yield status");
    check(L, nres == 2 && lua_gettop(co) == 2,
	  "lua_resume54 yield result count");
    check_string(co, 1, "y1", "lua_resume54 yield result #1");
    check_string(co, 2, "y2", "lua_resume54 yield result #2");
  }
  lua_pop(L, 1);

  co = lua_newthread(L);
  lua_pushcfunction(L, yield_two);
  lua_xmove(L, co, 1);
  {
    int nres = -1;
    check(L, lua_resume_sig(co, NULL, 0, &nres) == LUA_YIELD,
	  "lua_resume54 NULL from yield status");
    check(L, nres == 2 && lua_gettop(co) == 2,
	  "lua_resume54 NULL from yield count");
    check_string(co, 1, "y1", "lua_resume54 NULL from yield result #1");
    check_string(co, 2, "y2", "lua_resume54 NULL from yield result #2");
    lua_settop(co, 0);
  }
  lua_pop(L, 1);

  co = lua_newthread(L);
  lua_pushcfunction(L, yield_with_reyield_cont);
  lua_xmove(L, co, 1);
  {
    int nres = -1;
    yieldk_reyield_cont_called = 0;
    check(L, lua_resume_sig(co, L, 0, &nres) == LUA_YIELD,
	  "lua_yieldk reyield initial status");
    check(L, nres == 1 && lua_gettop(co) == 1,
	  "lua_yieldk reyield initial count");
    check_string(co, 1, "yield-before-cont",
	  "lua_yieldk reyield initial result");
    lua_settop(co, 0);
    lua_pushliteral(co, "resume-1");
    nres = -1;
    check(L, lua_resume_sig(co, L, 1, &nres) == LUA_YIELD,
	  "lua_yieldk reyield continuation yield status");
    check(L, yieldk_reyield_cont_called == 1,
	  "lua_yieldk reyield first continuation called");
    check(L, nres == 1 && lua_gettop(co) == 1,
	  "lua_yieldk reyield continuation yield count");
    check_string(co, 1, "yield-from-cont",
	  "lua_yieldk reyield continuation yield result");
    lua_settop(co, 0);
    lua_pushliteral(co, "resume-2");
    nres = -1;
    check(L, lua_resume_sig(co, L, 1, &nres) == LUA_OK,
	  "lua_yieldk reyield final status");
    check(L, yieldk_reyield_cont_called == 2,
	  "lua_yieldk reyield second continuation called");
    check(L, nres == 1 && lua_gettop(co) == 1,
	  "lua_yieldk reyield final count");
    check_string(co, 1, "reyield-final",
	  "lua_yieldk reyield final result");
  }
  lua_pop(L, 1);

  co = lua_newthread(L);
  lua_pushcfunction(L, callk_yield_driver);
  lua_xmove(L, co, 1);
  {
    int nres = -1;
    callk_yield_cont_called = 0;
    check(L, lua_resume_sig(co, L, 0, &nres) == LUA_YIELD,
	  "lua_callk yielding initial status");
    check(L, nres == 1 && lua_gettop(co) == 1,
	  "lua_callk yielding initial count");
    check_string(co, 1, "callk-yield", "lua_callk yielding result");
    lua_settop(co, 0);
    lua_pushliteral(co, "callk-resume");
    nres = -1;
    check(L, lua_resume_sig(co, L, 1, &nres) == LUA_OK,
	  "lua_callk yielding final status");
    check(L, callk_yield_cont_called == 1,
	  "lua_callk yielding continuation called");
    check(L, nres == 1 && lua_gettop(co) == 1,
	  "lua_callk yielding final count");
    check_string(co, 1, "callk-cont-result",
	  "lua_callk yielding final result");
  }
  lua_pop(L, 1);

  co = lua_newthread(L);
  lua_pushcfunction(L, callk_reyield_driver);
  lua_xmove(L, co, 1);
  {
    int nres = -1;
    callk_reyield_cont_called = 0;
    check(L, lua_resume_sig(co, L, 0, &nres) == LUA_YIELD,
	  "lua_callk reyield initial status");
    check(L, nres == 1 && lua_gettop(co) == 1,
	  "lua_callk reyield initial count");
    check_string(co, 1, "callk-callee-yield",
	  "lua_callk reyield initial result");
    lua_settop(co, 0);
    lua_pushliteral(co, "callk-resume-1");
    nres = -1;
    check(L, lua_resume_sig(co, L, 1, &nres) == LUA_YIELD,
	  "lua_callk reyield continuation status");
    check(L, callk_reyield_cont_called == 1,
	  "lua_callk reyield first continuation called");
    check(L, nres == 1 && lua_gettop(co) == 1,
	  "lua_callk reyield continuation count");
    check_string(co, 1, "callk-yield-from-cont",
	  "lua_callk reyield continuation result");
    lua_settop(co, 0);
    lua_pushliteral(co, "callk-resume-2");
    nres = -1;
    check(L, lua_resume_sig(co, L, 1, &nres) == LUA_OK,
	  "lua_callk reyield final status");
    check(L, callk_reyield_cont_called == 2,
	  "lua_callk reyield second continuation called");
    check(L, nres == 1 && lua_gettop(co) == 1,
	  "lua_callk reyield final count");
    check_string(co, 1, "callk-reyield-final",
	  "lua_callk reyield final result");
  }
  lua_pop(L, 1);

  co = lua_newthread(L);
  lua_pushcfunction(L, callk_multret_driver);
  lua_xmove(L, co, 1);
  {
    int nres = -1;
    callk_multret_cont_called = 0;
    check(L, lua_resume_sig(co, L, 0, &nres) == LUA_YIELD,
	  "lua_callk multret initial status");
    check(L, nres == 1 && lua_gettop(co) == 1,
	  "lua_callk multret initial count");
    check_string(co, 1, "callk-multret-yield",
	  "lua_callk multret initial result");
    lua_settop(co, 0);
    nres = -1;
    check(L, lua_resume_sig(co, L, 0, &nres) == LUA_OK,
	  "lua_callk multret final status");
    check(L, callk_multret_cont_called == 1,
	  "lua_callk multret continuation called");
    check(L, nres == 1 && lua_gettop(co) == 1,
	  "lua_callk multret final count");
    check_string(co, 1, "callk-multret-cont",
	  "lua_callk multret final result");
  }
  lua_pop(L, 1);

  co = lua_newthread(L);
  lua_pushcfunction(L, pcallk_yield_driver);
  lua_xmove(L, co, 1);
  {
    int nres = -1;
    pcallk_yield_cont_called = 0;
    check(L, lua_resume_sig(co, L, 0, &nres) == LUA_YIELD,
	  "lua_pcallk yielding initial status");
    check(L, nres == 1 && lua_gettop(co) == 1,
	  "lua_pcallk yielding initial count");
    check_string(co, 1, "pcallk-yield", "lua_pcallk yielding result");
    lua_settop(co, 0);
    lua_pushliteral(co, "pcallk-resume");
    nres = -1;
    check(L, lua_resume_sig(co, L, 1, &nres) == LUA_OK,
	  "lua_pcallk yielding final status");
    check(L, pcallk_yield_cont_called == 1,
	  "lua_pcallk yielding continuation called");
    check(L, nres == 1 && lua_gettop(co) == 1,
	  "lua_pcallk yielding final count");
    check_string(co, 1, "pcallk-cont-result",
	  "lua_pcallk yielding final result");
  }
  lua_pop(L, 1);

  co = lua_newthread(L);
  lua_pushcfunction(L, pcallk_multret_driver);
  lua_xmove(L, co, 1);
  {
    int nres = -1;
    pcallk_multret_cont_called = 0;
    check(L, lua_resume_sig(co, L, 0, &nres) == LUA_YIELD,
	  "lua_pcallk multret initial status");
    check(L, nres == 1 && lua_gettop(co) == 1,
	  "lua_pcallk multret initial count");
    check_string(co, 1, "pcallk-multret-yield",
	  "lua_pcallk multret initial result");
    lua_settop(co, 0);
    nres = -1;
    check(L, lua_resume_sig(co, L, 0, &nres) == LUA_OK,
	  "lua_pcallk multret final status");
    check(L, pcallk_multret_cont_called == 1,
	  "lua_pcallk multret continuation called");
    check(L, nres == 1 && lua_gettop(co) == 1,
	  "lua_pcallk multret final count");
    check_string(co, 1, "pcallk-multret-cont",
	  "lua_pcallk multret final result");
  }
  lua_pop(L, 1);

  co = lua_newthread(L);
  lua_pushcfunction(L, pcallk_reyield_driver);
  lua_xmove(L, co, 1);
  {
    int nres = -1;
    pcallk_reyield_cont_called = 0;
    check(L, lua_resume_sig(co, L, 0, &nres) == LUA_YIELD,
	  "lua_pcallk reyield initial status");
    check(L, nres == 1 && lua_gettop(co) == 1,
	  "lua_pcallk reyield initial count");
    check_string(co, 1, "pcallk-callee-yield",
	  "lua_pcallk reyield initial result");
    lua_settop(co, 0);
    lua_pushliteral(co, "pcallk-resume-1");
    nres = -1;
    check(L, lua_resume_sig(co, L, 1, &nres) == LUA_YIELD,
	  "lua_pcallk reyield continuation status");
    check(L, pcallk_reyield_cont_called == 1,
	  "lua_pcallk reyield first continuation called");
    check(L, nres == 1 && lua_gettop(co) == 1,
	  "lua_pcallk reyield continuation count");
    check_string(co, 1, "pcallk-yield-from-cont",
	  "lua_pcallk reyield continuation result");
    lua_settop(co, 0);
    lua_pushliteral(co, "pcallk-resume-2");
    nres = -1;
    check(L, lua_resume_sig(co, L, 1, &nres) == LUA_OK,
	  "lua_pcallk reyield final status");
    check(L, pcallk_reyield_cont_called == 2,
	  "lua_pcallk reyield second continuation called");
    check(L, nres == 1 && lua_gettop(co) == 1,
	  "lua_pcallk reyield final count");
    check_string(co, 1, "pcallk-reyield-final",
	  "lua_pcallk reyield final result");
  }
  lua_pop(L, 1);

  co = lua_newthread(L);
  lua_pushcfunction(L, callk_yield_error_driver);
  lua_xmove(L, co, 1);
  {
    int nres = -1;
    callk_yield_error_cont_called = 0;
    check(L, lua_resume_sig(co, L, 0, &nres) == LUA_YIELD,
	  "lua_callk yield-error initial status");
    check(L, nres == 1 && lua_gettop(co) == 1,
	  "lua_callk yield-error initial count");
    check_string(co, 1, "callk-error-yield",
	  "lua_callk yield-error result");
    lua_settop(co, 0);
    nres = -1;
    check(L, lua_resume_sig(co, L, 0, &nres) == LUA_ERRRUN,
	  "lua_callk yield-error final status");
    check(L, callk_yield_error_cont_called == 0,
	  "lua_callk yield-error continuation not called");
    check(L, nres == 1 && lua_gettop(co) == 1,
	  "lua_callk yield-error final count");
    check_string(co, 1, "callk-error-after-yield",
	  "lua_callk yield-error final object");
  }
  lua_pop(L, 1);

  co = lua_newthread(L);
  lua_pushcfunction(L, pcallk_yield_error_driver);
  lua_xmove(L, co, 1);
  {
    int nres = -1;
    pcallk_yield_error_cont_called = 0;
    check(L, lua_resume_sig(co, L, 0, &nres) == LUA_YIELD,
	  "lua_pcallk yield-error initial status");
    check(L, nres == 1 && lua_gettop(co) == 1,
	  "lua_pcallk yield-error initial count");
    check_string(co, 1, "pcallk-error-yield",
	  "lua_pcallk yield-error result");
    lua_settop(co, 0);
    nres = -1;
    check(L, lua_resume_sig(co, L, 0, &nres) == LUA_OK,
	  "lua_pcallk yield-error final status");
    check(L, pcallk_yield_error_cont_called == 1,
	  "lua_pcallk yield-error continuation called");
    check(L, nres == 1 && lua_gettop(co) == 1,
	  "lua_pcallk yield-error final count");
    check_string(co, 1, "pcallk-error-handled",
	  "lua_pcallk yield-error final result");
  }
  lua_pop(L, 1);

  co = lua_newthread(L);
  lua_pushcfunction(L, pcallk_msgh_yield_error_driver);
  lua_xmove(L, co, 1);
  {
    int nres = -1;
    pcallk_msgh_yield_error_cont_called = 0;
    check(L, lua_resume_sig(co, L, 0, &nres) == LUA_YIELD,
	  "lua_pcallk message handler yield-error initial status");
    check(L, nres == 1 && lua_gettop(co) == 1,
	  "lua_pcallk message handler yield-error initial count");
    check_string(co, 1, "pcallk-msgh-error-yield",
	  "lua_pcallk message handler yield-error result");
    lua_gc(L, LUA_GCCOLLECT, 0);
    lua_settop(co, 0);
    nres = -1;
    check(L, lua_resume_sig(co, L, 0, &nres) == LUA_OK,
	  "lua_pcallk message handler yield-error final status");
    check(L, pcallk_msgh_yield_error_cont_called == 1,
	  "lua_pcallk message handler yield-error continuation called");
    check(L, nres == 1 && lua_gettop(co) == 1,
	  "lua_pcallk message handler yield-error final count");
    check_string(co, 1, "pcallk-msgh-error-handled",
	  "lua_pcallk message handler yield-error final result");
  }
  lua_pop(L, 1);
  lua_gc(L, LUA_GCCOLLECT, 0);
  co = lua_newthread(L);
  check(L, co != NULL,
	"lua_pcallk message handler yield-error stack growth cleanup");
  lua_pop(L, 1);

  co = lua_newthread(L);
  lua_pushcfunction(L, pcallk_msgh_error_yield_error_driver);
  lua_xmove(L, co, 1);
  {
    int nres = -1;
    pcallk_msgh_error_yield_error_cont_called = 0;
    check(L, lua_resume_sig(co, L, 0, &nres) == LUA_YIELD,
	  "lua_pcallk message handler error initial status");
    check(L, nres == 1 && lua_gettop(co) == 1,
	  "lua_pcallk message handler error initial count");
    check_string(co, 1, "pcallk-msgh-error-handler-yield",
	  "lua_pcallk message handler error yield result");
    lua_settop(co, 0);
    nres = -1;
    check(L, lua_resume_sig(co, L, 0, &nres) == LUA_OK,
	  "lua_pcallk message handler error final status");
    check(L, pcallk_msgh_error_yield_error_cont_called == 1,
	  "lua_pcallk message handler error continuation called");
    check(L, nres == 1 && lua_gettop(co) == 1,
	  "lua_pcallk message handler error final count");
    check_string(co, 1, "pcallk-msgh-error-error-handled",
	  "lua_pcallk message handler error final result");
  }
  lua_pop(L, 1);
  lua_gc(L, LUA_GCCOLLECT, 0);
  co = lua_newthread(L);
  check(L, co != NULL,
	"lua_pcallk message handler error stack growth cleanup");
  lua_pop(L, 1);

  co = lua_newthread(L);
  lua_pushcfunction(L, yield_with_cont);
  lua_xmove(L, co, 1);
  {
    int nres = -1;
    yieldk_cont_called = 0;
    check(L, lua_resume_sig(co, L, 0, &nres) == LUA_YIELD,
	  "lua_yieldk continuation initial yield status");
    check(L, nres == 1 && lua_gettop(co) == 1,
	  "lua_yieldk continuation yield result count");
    check_string(co, 1, "yield-result",
	  "lua_yieldk continuation yield result");
    lua_settop(co, 0);
    lua_pushliteral(co, "resume-arg");
    nres = -1;
    check(L, lua_resume_sig(co, L, 1, &nres) == LUA_OK,
	  "lua_yieldk continuation resume status");
    check(L, yieldk_cont_called == 1, "lua_yieldk continuation called");
    check(L, nres == 1 && lua_gettop(co) == 1,
	  "lua_yieldk continuation final result count");
    check_string(co, 1, "cont-result",
	  "lua_yieldk continuation final result");
  }
  lua_pop(L, 1);

  co = lua_newthread(L);
  lua_pushcfunction(L, yield_with_error_cont);
  lua_xmove(L, co, 1);
  {
    int nres = -1;
    check(L, lua_resume_sig(co, L, 0, &nres) == LUA_YIELD,
	  "lua_yieldk error continuation initial yield");
    check(L, nres == 1 && lua_gettop(co) == 1,
	  "lua_yieldk error continuation yield count");
    lua_settop(co, 0);
    nres = -1;
    check(L, lua_resume_sig(co, L, 0, &nres) == LUA_ERRRUN,
	  "lua_yieldk error continuation resume status");
    check(L, nres == 1 && lua_gettop(co) == 1,
	  "lua_yieldk error continuation error count");
    check_string(co, 1, "yieldk cont boom",
	  "lua_yieldk error continuation message");
  }
  lua_pop(L, 1);

  co = lua_newthread(L);
  lua_pushcfunction(L, yield_with_cont);
  lua_xmove(L, co, 1);
  {
    int nres = -1;
    check(L, lua_resume_sig(co, L, 0, &nres) == LUA_YIELD,
	  "lua_yieldk continuation close setup");
    check(L, lua_closethread(co, L) == LUA_OK,
	  "lua_yieldk continuation closethread status");
    check(L, lua_status(co) == LUA_OK && lua_gettop(co) == 0,
	  "lua_yieldk continuation closethread clears state");
  }
  lua_pop(L, 1);

  co = lua_newthread(L);
  check(L, luaL_loadstring(co, "error('resume-boom', 0)") == LUA_OK,
	"lua_resume54 execution error load");
  {
    int nres = -1;
    check(L, lua_resume_sig(co, L, 0, &nres) == LUA_ERRRUN,
	  "lua_resume54 execution error status");
    check(L, nres == 1 && lua_gettop(co) == 1,
	  "lua_resume54 execution error count");
    check_string(co, 1, "resume-boom",
	  "lua_resume54 execution error object");
  }
  lua_pop(L, 1);

  co = lua_newthread(L);
  lua_pushcfunction(L, return_two);
  lua_xmove(L, co, 1);
  {
    int nres = -1;
    check(L, lua_resume_sig(co, L, 0, &nres) == LUA_OK,
	  "lua_resume function pointer return status");
    check(L, nres == 2 && lua_gettop(co) == 2,
	  "lua_resume54 return result count");
    check_string(co, 1, "r1", "lua_resume54 return result #1");
    check_string(co, 2, "r2", "lua_resume54 return result #2");
    lua_settop(co, 0);
    nres = -1;
    check(L, lua_resume_sig(co, L, 0, &nres) == LUA_ERRRUN,
	  "lua_resume54 dead coroutine status");
    check(L, nres == 0 && lua_gettop(co) == 1,
	  "lua_resume54 dead coroutine error count");
    check(L, strstr(lua_tostring(co, 1), "dead coroutine") != NULL,
	  "lua_resume54 dead coroutine error text");
  }
  lua_pop(L, 1);

  co = lua_newthread(L);
  lua_pushcfunction(L, return_two);
  lua_xmove(L, co, 1);
  {
    int nres = -1;
    check(L, lua_resume_sig(co, NULL, 0, &nres) == LUA_OK,
	  "lua_resume54 NULL from return status");
    check(L, nres == 2 && lua_gettop(co) == 2,
	  "lua_resume54 NULL from return count");
    check_string(co, 1, "r1", "lua_resume54 NULL from return result #1");
    check_string(co, 2, "r2", "lua_resume54 NULL from return result #2");
  }
  lua_pop(L, 1);
}

static void test_compare_len_arith(lua_State *L)
{
  static const char pointer_key;
  int rtype;
  int status;
  const char *errmsg;
  RawGetI54Sig rawgeti_compat_sig = lua_rawgeti54;
  RawSetI54Sig rawseti_compat_sig = lua_rawseti54;

  (void)rawgeti_compat_sig;
  (void)rawseti_compat_sig;

  lua_pushinteger(L, 2);
  lua_pushinteger(L, 3);
  check(L, lua_compare(L, -2, -1, LUA_OPLT), "lua_compare lt");
  check(L, lua_compare(L, -2, -1, LUA_OPLE), "lua_compare le");
  check(L, !lua_compare(L, -2, -1, LUA_OPEQ), "lua_compare eq false");
  lua_pop(L, 2);

  lua_pushliteral(L, "a\0b");
  check(L, lua_rawlen(L, -1) == 1,
	"Lua 5.4 lua_pushliteral macro uses lua_pushstring semantics");
  lua_pop(L, 1);

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
  if (sizeof(lua_Integer) > sizeof(int)) {
    lua_newtable(L);
    lua_newtable(L);
    lua_pushcfunction(L, len_wide_meta);
    lua_setfield(L, -2, "__len");
    lua_setmetatable(L, -2);
    lua_len(L, -1);
    check_integer(L, -1, (lua_Integer)1024 * 1024 * 1024 * 1024,
		  "lua_len wider 64-bit __len metamethod");
    lua_pop(L, 2);
  }

  lua_pushinteger(L, 5);
  lua_pushinteger(L, 2);
  lua_arith(L, LUA_OPADD);
  check(L, lua_isinteger(L, -1), "lua_arith integer add subtype");
  check_integer(L, -1, 7, "lua_arith add");
  lua_pop(L, 1);

  lua_pushcfunction(L, arith_missing_binary_operand);
  status = lua_pcall(L, 0, 0, 0);
  check(L, status == LUA_ERRRUN, "lua_arith rejects missing binary operand");
  check(L, strstr(lua_tostring(L, -1), "invalid value") != NULL,
	"lua_arith missing binary operand error");
  lua_pop(L, 1);

  lua_pushcfunction(L, arith_missing_unary_operand);
  status = lua_pcall(L, 0, 0, 0);
  check(L, status == LUA_ERRRUN, "lua_arith rejects missing unary operand");
  check(L, strstr(lua_tostring(L, -1), "invalid value") != NULL,
	"lua_arith missing unary operand error");
  lua_pop(L, 1);

  lua_pushcfunction(L, arith_invalid_op);
  status = lua_pcall(L, 0, 0, 0);
  check(L, status == LUA_ERRRUN, "lua_arith rejects invalid op");
  check(L, strstr(lua_tostring(L, -1), "invalid value") != NULL,
	"lua_arith invalid op error");
  lua_pop(L, 1);

  lua_pushnumber(L, (lua_Number)3);
  check(L, !lua_isinteger(L, -1), "lua_isinteger float subtype");
  lua_pop(L, 1);

  lua_pushliteral(L, "1");
  lua_pushinteger(L, 2);
  lua_arith(L, LUA_OPADD);
  check(L, lua_isinteger(L, -1), "lua_arith string integer add subtype");
  check_integer(L, -1, 3, "lua_arith string integer add");
  lua_pop(L, 1);

  lua_pushliteral(L, "1.0");
  lua_pushinteger(L, 2);
  lua_arith(L, LUA_OPADD);
  check(L, !lua_isinteger(L, -1) && lua_tonumber(L, -1) == (lua_Number)3,
	"lua_arith string float add subtype");
  lua_pop(L, 1);

  lua_pushliteral(L, "");
  check(L, lua_getmetatable(L, -1) == 1,
	"lua_arith string add metatable available");
  lua_pushcfunction(L, arith_string_add_meta);
  lua_setfield(L, -2, "__add");
  lua_pushliteral(L, "1");
  lua_pushinteger(L, 2);
  lua_arith(L, LUA_OPADD);
  check_string(L, -1, "api-add-meta",
	       "lua_arith string add uses string metatable");
  lua_pop(L, 1);
  lua_pushnil(L);
  lua_setfield(L, -2, "__add");
  lua_pop(L, 2);

  lua_pushinteger(L, 5);
  lua_pushinteger(L, 2);
  lua_arith(L, LUA_OPMOD);
  check(L, lua_isinteger(L, -1), "lua_arith integer mod subtype");
  check_integer(L, -1, 1, "lua_arith integer mod");
  lua_pop(L, 1);

  lua_pushinteger(L, 7);
  lua_pushinteger(L, 3);
  lua_arith(L, LUA_OPIDIV);
  check_integer(L, -1, 2, "lua_arith idiv");
  lua_pop(L, 1);

  lua_pushliteral(L, "");
  check(L, lua_getmetatable(L, -1) == 1,
	"lua_arith string idiv metatable available");
  lua_pushcfunction(L, arith_string_idiv_meta);
  lua_setfield(L, -2, "__idiv");
  lua_pushliteral(L, "7");
  lua_pushinteger(L, 3);
  lua_arith(L, LUA_OPIDIV);
  check_string(L, -1, "api-idiv-meta",
	       "lua_arith string idiv uses string metatable");
  lua_pop(L, 1);
  lua_pushnil(L);
  lua_setfield(L, -2, "__idiv");
  lua_pop(L, 2);

  lua_pushinteger(L, 2);
  lua_pushinteger(L, 3);
  lua_arith(L, LUA_OPPOW);
  check(L, !lua_isinteger(L, -1) && lua_tonumber(L, -1) == (lua_Number)8,
	"lua_arith pow returns float subtype");
  lua_pop(L, 1);

  if (sizeof(lua_Integer) > sizeof(int)) {
    lua_Integer big40 = (lua_Integer)1024 * 1024 * 1024 * 1024;
    lua_Integer big53 = (((lua_Integer)1) << 53) + 1;
    lua_pushinteger(L, big40);
    lua_pushinteger(L, 3);
    lua_arith(L, LUA_OPADD);
    check_integer(L, -1, big40 + 3,
		  "lua_arith preserves wider 64-bit add result");
    lua_pop(L, 1);
    lua_pushinteger(L, LUA_MAXINTEGER);
    lua_pushinteger(L, 1);
    lua_arith(L, LUA_OPADD);
    check_integer(L, -1, LUA_MININTEGER,
		  "lua_arith wraps LUA_MAXINTEGER add result");
    lua_pop(L, 1);
    lua_pushinteger(L, big40);
    lua_pushinteger(L, 3);
    lua_arith(L, LUA_OPMUL);
    check_integer(L, -1, big40 * 3,
		  "lua_arith preserves wider 64-bit multiply result");
    lua_pop(L, 1);
    lua_pushinteger(L, LUA_MININTEGER);
    lua_pushinteger(L, 1);
    lua_arith(L, LUA_OPSUB);
    check_integer(L, -1, LUA_MAXINTEGER,
		  "lua_arith wraps LUA_MININTEGER subtract result");
    lua_pop(L, 1);
    lua_pushinteger(L, big40 + 7);
    lua_pushinteger(L, 4);
    lua_arith(L, LUA_OPIDIV);
    check_integer(L, -1, (big40 + 7) / 4,
		  "lua_arith preserves wider 64-bit idiv result");
    lua_pop(L, 1);
    lua_pushinteger(L, LUA_MININTEGER);
    lua_pushinteger(L, -1);
    lua_arith(L, LUA_OPIDIV);
    check_integer(L, -1, LUA_MININTEGER,
		  "lua_arith wraps LUA_MININTEGER idiv by -1");
    lua_pop(L, 1);
    lua_pushinteger(L, big40 + 7);
    lua_pushinteger(L, 4);
    lua_arith(L, LUA_OPMOD);
    check_integer(L, -1, 3,
		  "lua_arith preserves wider 64-bit mod result");
    lua_pop(L, 1);
    lua_pushinteger(L, LUA_MININTEGER);
    lua_pushinteger(L, -1);
    lua_arith(L, LUA_OPMOD);
    check_integer(L, -1, 0, "lua_arith LUA_MININTEGER mod by -1");
    lua_pop(L, 1);
    lua_pushliteral(L, "9007199254740993");
    lua_pushinteger(L, 0);
    lua_arith(L, LUA_OPADD);
    check_integer(L, -1, big53,
		  "lua_arith preserves wider string add result");
    lua_pop(L, 1);
    lua_pushliteral(L, "9007199254740993");
    lua_pushinteger(L, 1);
    lua_arith(L, LUA_OPIDIV);
    check_integer(L, -1, big53,
		  "lua_arith preserves wider string idiv result");
    lua_pop(L, 1);
    lua_pushliteral(L, "9007199254740993");
    lua_pushinteger(L, 10);
    lua_arith(L, LUA_OPMOD);
    check_integer(L, -1, 3,
		  "lua_arith preserves wider string mod result");
    lua_pop(L, 1);
    lua_pushinteger(L, big40);
    lua_arith(L, LUA_OPUNM);
    check_integer(L, -1, -big40,
		  "lua_arith preserves wider 64-bit unary minus result");
    lua_pop(L, 1);
    lua_pushinteger(L, LUA_MININTEGER);
    lua_arith(L, LUA_OPUNM);
    check_integer(L, -1, LUA_MININTEGER,
		  "lua_arith wraps LUA_MININTEGER unary minus");
    lua_pop(L, 1);
    lua_pushliteral(L, "1099511627776");
    lua_arith(L, LUA_OPUNM);
    check_integer(L, -1, -big40,
		  "lua_arith preserves wider string unary minus result");
    lua_pop(L, 1);
    lua_pushinteger(L, big40);
    lua_pushinteger(L, 1);
    lua_arith(L, LUA_OPPOW);
    check(L, !lua_isinteger(L, -1) &&
	     lua_tonumber(L, -1) == (lua_Number)big40,
	  "lua_arith wider 64-bit pow returns float subtype");
    lua_pop(L, 1);
    lua_pushinteger(L, big40);
    lua_pushinteger(L, big40 + 0x123);
    lua_arith(L, LUA_OPBAND);
    check_integer(L, -1, big40,
		  "lua_arith preserves wider 64-bit band result");
    lua_pop(L, 1);
    lua_pushinteger(L, big40);
    lua_pushinteger(L, 0xff);
    lua_arith(L, LUA_OPBOR);
    check_integer(L, -1, big40 + 0xff,
		  "lua_arith preserves wider 64-bit bor result");
    lua_pop(L, 1);
    lua_pushinteger(L, big40 + 3);
    lua_pushinteger(L, big40 + 1);
    lua_arith(L, LUA_OPBXOR);
    check_integer(L, -1, 2,
		  "lua_arith preserves wider 64-bit bxor result");
    lua_pop(L, 1);
    lua_pushinteger(L, 1);
    lua_pushinteger(L, 40);
    lua_arith(L, LUA_OPSHL);
    check_integer(L, -1, big40,
		  "lua_arith preserves wider 64-bit shl result");
    lua_pop(L, 1);
    lua_pushinteger(L, big40);
    lua_pushinteger(L, 40);
    lua_arith(L, LUA_OPSHR);
    check_integer(L, -1, 1,
		  "lua_arith preserves wider 64-bit shr result");
    lua_pop(L, 1);
    lua_pushinteger(L, big40);
    lua_pushinteger(L, -40);
    lua_arith(L, LUA_OPSHL);
    check_integer(L, -1, 1,
		  "lua_arith preserves wider 64-bit negative shift result");
    lua_pop(L, 1);

    lua_pushcfunction(L, arith_string_bor);
    status = lua_pcall(L, 0, 1, 0);
    check(L, status == LUA_ERRRUN,
	  "lua_arith string bitwise rejects string");
    errmsg = lua_tostring(L, -1);
    check(L, errmsg != NULL &&
	     strstr(errmsg, "bitwise operation") != NULL &&
	     strstr(errmsg, "string value") != NULL,
	  "lua_arith string bitwise error text");
    lua_pop(L, 1);

    lua_pushcfunction(L, arith_string_bnot);
    status = lua_pcall(L, 0, 1, 0);
    check(L, status == LUA_ERRRUN,
	  "lua_arith string bnot rejects string");
    errmsg = lua_tostring(L, -1);
    check(L, errmsg != NULL &&
	     strstr(errmsg, "bitwise operation") != NULL &&
	     strstr(errmsg, "string value") != NULL,
	  "lua_arith string bnot error text");
    lua_pop(L, 1);

    lua_pushliteral(L, "");
    check(L, lua_getmetatable(L, -1) == 1,
	  "lua_arith string metatable available");
    lua_pushcfunction(L, arith_string_bor_meta);
    lua_setfield(L, -2, "__bor");
    lua_pushliteral(L, "1099511627776");
    lua_pushinteger(L, 7);
    lua_arith(L, LUA_OPBOR);
    check_string(L, -1, "api-bor-meta",
		 "lua_arith string bitwise uses string metatable");
    lua_pop(L, 1);
    lua_pushnil(L);
    lua_setfield(L, -2, "__bor");
    lua_pop(L, 2);

    lua_pushinteger(L, big40);
    lua_arith(L, LUA_OPBNOT);
    check_integer(L, -1, (lua_Integer)~(lua_Unsigned)big40,
		  "lua_arith preserves wider 64-bit bnot result");
    lua_pop(L, 1);
    lua_pushinteger(L, LUA_MAXINTEGER);
    lua_arith(L, LUA_OPBNOT);
    check_integer(L, -1, LUA_MININTEGER,
		  "lua_arith bnot handles LUA_MAXINTEGER");
    lua_pop(L, 1);
  }

  lua_pushinteger(L, 6);
  lua_pushinteger(L, 3);
  lua_arith(L, LUA_OPBAND);
  check_integer(L, -1, 2, "lua_arith band");
  lua_pop(L, 1);

  lua_newtable(L);
  lua_pushliteral(L, "field-value");
  lua_setfield(L, -2, "field");
  lua_pushliteral(L, "index-value");
  lua_seti(L, -2, 7);
  lua_pushliteral(L, "raw-value");
  lua_rawseti_sig(L, -2, 8);
  lua_pushliteral(L, "ptr-value");
  lua_rawsetp(L, -2, &pointer_key);

  rtype = lua_getfield_sig(L, -1, "field");
  check(L, rtype == LUA_TSTRING, "lua_getfield function pointer return type");
  check_string(L, -1, "field-value", "lua_getfield return value");
  lua_pop(L, 1);

  rtype = lua_geti_sig(L, -1, 7);
  check(L, rtype == LUA_TSTRING, "lua_geti function pointer return type");
  check_string(L, -1, "index-value", "lua_geti return value");
  lua_pop(L, 1);

  lua_pushliteral(L, "field");
  rtype = lua_gettable_sig(L, -2);
  check(L, rtype == LUA_TSTRING, "lua_gettable function pointer return type");
  check_string(L, -1, "field-value", "lua_gettable return value");
  lua_pop(L, 1);

  lua_pushinteger(L, 8);
  rtype = lua_rawget_sig(L, -2);
  check(L, rtype == LUA_TSTRING, "lua_rawget function pointer return type");
  check_string(L, -1, "raw-value", "lua_rawget return value");
  lua_pop(L, 1);

  rtype = lua_rawgeti_sig(L, -1, 8);
  check(L, rtype == LUA_TSTRING, "lua_rawgeti function pointer return type");
  check_string(L, -1, "raw-value", "lua_rawgeti return value");
  lua_pop(L, 1);

  if (sizeof(lua_Integer) > sizeof(int)) {
    lua_Integer big = (lua_Integer)((lua_Unsigned)0x7fffffffu + 1u);
    lua_Integer big2 = big + 1;
    lua_Integer big40 = (lua_Integer)1024 * 1024 * 1024 * 1024;
    lua_Integer big40b = big40 + 1;
    lua_Integer big53 = (((lua_Integer)1) << 53) + 1;
    lua_Integer neg40 = -big40;
    lua_Integer neg40b = neg40 - 1;
    lua_Number rounded53 = (lua_Number)big53;
    lua_Number pos63 = -(lua_Number)LUA_MININTEGER;
    lua_Number nanv = (lua_Number)HUGE_VAL - (lua_Number)HUGE_VAL;
    lua_pushinteger(L, big40);
    lua_pushinteger(L, big40b);
    check(L, lua_compare(L, -2, -1, LUA_OPLT),
	  "lua_compare accepts wider 64-bit C integer lt");
    check(L, lua_compare(L, -2, -1, LUA_OPLE),
	  "lua_compare accepts wider 64-bit C integer le");
    check(L, !lua_compare(L, -2, -1, LUA_OPEQ),
	  "lua_compare accepts wider 64-bit C integer eq false");
    lua_pop(L, 2);
    lua_pushinteger(L, big40);
    lua_pushnumber(L, (lua_Number)big40);
    check(L, lua_compare(L, -2, -1, LUA_OPEQ),
	  "lua_compare matches wider integer and exact number");
    lua_pop(L, 2);
    lua_pushinteger(L, big53);
    lua_pushnumber(L, rounded53);
    check(L, !lua_compare(L, -2, -1, LUA_OPEQ),
	  "lua_compare rejects rounded int64/double equality");
    check(L, !lua_compare(L, -2, -1, LUA_OPLT),
	  "lua_compare rounded int64/double lt false");
    check(L, !lua_compare(L, -2, -1, LUA_OPLE),
	  "lua_compare rounded int64/double le false");
    check(L, lua_compare(L, -1, -2, LUA_OPLT),
	  "lua_compare rounded double less than int64");
    check(L, lua_compare(L, -1, -2, LUA_OPLE),
	  "lua_compare rounded double le int64");
    lua_pop(L, 2);
    lua_pushinteger(L, LUA_MAXINTEGER);
    lua_pushnumber(L, pos63);
    check(L, !lua_compare(L, -2, -1, LUA_OPEQ),
	  "lua_compare maxinteger not equal upper-exclusive double");
    check(L, lua_compare(L, -2, -1, LUA_OPLT),
	  "lua_compare maxinteger less than upper-exclusive double");
    check(L, lua_compare(L, -2, -1, LUA_OPLE),
	  "lua_compare maxinteger le upper-exclusive double");
    lua_pop(L, 2);
    lua_pushinteger(L, big40);
    lua_pushnumber(L, nanv);
    check(L, !lua_compare(L, -2, -1, LUA_OPEQ),
	  "lua_compare int64 not equal NaN");
    check(L, !lua_compare(L, -2, -1, LUA_OPLT),
	  "lua_compare int64 less than NaN false");
    check(L, !lua_compare(L, -2, -1, LUA_OPLE),
	  "lua_compare int64 le NaN false");
    check(L, !lua_compare(L, -1, -2, LUA_OPLT),
	  "lua_compare NaN less than int64 false");
    lua_pop(L, 2);
    lua_pushinteger(L, big);
    lua_pushliteral(L, "geti-big-generic");
    lua_rawset(L, -3);
    rtype = lua_geti_sig(L, -1, big);
    check(L, rtype == LUA_TSTRING,
	  "lua_geti accepts 64-bit C integer key");
    check_string(L, -1, "geti-big-generic", "lua_geti 64-bit key value");
    lua_pop(L, 1);
    lua_pushliteral(L, "seti-big-generic");
    lua_seti(L, -2, big2);
    lua_pushinteger(L, big2);
    rtype = lua_rawget_sig(L, -2);
    check(L, rtype == LUA_TSTRING,
	  "lua_seti accepts 64-bit C integer key");
    check_string(L, -1, "seti-big-generic", "lua_seti 64-bit key value");
    lua_pop(L, 1);
    lua_pushinteger(L, big);
    lua_pushliteral(L, "raw-big-generic");
    lua_rawset(L, -3);
    rtype = lua_rawgeti_sig(L, -1, big);
    check(L, rtype == LUA_TSTRING,
	  "lua_rawgeti accepts 64-bit C integer key");
    check_string(L, -1, "raw-big-generic", "lua_rawgeti 64-bit key value");
    lua_pop(L, 1);
    lua_pushliteral(L, "raw-big-seti");
    lua_rawseti_sig(L, -2, big);
    lua_pushinteger(L, big);
    rtype = lua_rawget_sig(L, -2);
    check(L, rtype == LUA_TSTRING,
	  "lua_rawseti accepts 64-bit C integer key");
    check_string(L, -1, "raw-big-seti", "lua_rawseti 64-bit key value");
    lua_pop(L, 1);
    lua_pushinteger(L, big40);
    lua_pushliteral(L, "geti-big40-generic");
    lua_rawset(L, -3);
    rtype = lua_geti_sig(L, -1, big40);
    check(L, rtype == LUA_TSTRING,
	  "lua_geti accepts wider 64-bit C integer key");
    check_string(L, -1, "geti-big40-generic",
		 "lua_geti wider 64-bit key value");
    lua_pop(L, 1);
    lua_pushliteral(L, "seti-big40-generic");
    lua_seti(L, -2, big40b);
    lua_pushinteger(L, big40b);
    rtype = lua_rawget_sig(L, -2);
    check(L, rtype == LUA_TSTRING,
	  "lua_seti accepts wider 64-bit C integer key");
    check_string(L, -1, "seti-big40-generic",
		 "lua_seti wider 64-bit key value");
    lua_pop(L, 1);
    lua_pushinteger(L, big40);
    lua_pushliteral(L, "raw-big40-generic");
    lua_rawset(L, -3);
    rtype = lua_rawgeti_sig(L, -1, big40);
    check(L, rtype == LUA_TSTRING,
	  "lua_rawgeti accepts wider 64-bit C integer key");
    check_string(L, -1, "raw-big40-generic",
		 "lua_rawgeti wider 64-bit key value");
    lua_pop(L, 1);
    lua_pushliteral(L, "raw-big40-seti");
    lua_rawseti_sig(L, -2, big40);
    lua_pushinteger(L, big40);
    rtype = lua_rawget_sig(L, -2);
    check(L, rtype == LUA_TSTRING,
	  "lua_rawseti accepts wider 64-bit C integer key");
    check_string(L, -1, "raw-big40-seti",
		 "lua_rawseti wider 64-bit key value");
    lua_pop(L, 1);
    lua_pushinteger(L, neg40);
    lua_pushliteral(L, "geti-neg40-generic");
    lua_rawset(L, -3);
    rtype = lua_geti_sig(L, -1, neg40);
    check(L, rtype == LUA_TSTRING,
	  "lua_geti accepts negative wider 64-bit C integer key");
    check_string(L, -1, "geti-neg40-generic",
		 "lua_geti negative wider 64-bit key value");
    lua_pop(L, 1);
    lua_pushliteral(L, "seti-neg40-generic");
    lua_seti(L, -2, neg40b);
    lua_pushinteger(L, neg40b);
    rtype = lua_rawget_sig(L, -2);
    check(L, rtype == LUA_TSTRING,
	  "lua_seti accepts negative wider 64-bit C integer key");
    check_string(L, -1, "seti-neg40-generic",
		 "lua_seti negative wider 64-bit key value");
    lua_pop(L, 1);
    lua_pushinteger(L, neg40);
    lua_pushliteral(L, "raw-neg40-generic");
    lua_rawset(L, -3);
    rtype = lua_rawgeti_sig(L, -1, neg40);
    check(L, rtype == LUA_TSTRING,
	  "lua_rawgeti accepts negative wider 64-bit C integer key");
    check_string(L, -1, "raw-neg40-generic",
		 "lua_rawgeti negative wider 64-bit key value");
    lua_pop(L, 1);
    lua_pushliteral(L, "raw-neg40-seti");
    lua_rawseti_sig(L, -2, neg40);
    lua_pushinteger(L, neg40);
    rtype = lua_rawget_sig(L, -2);
    check(L, rtype == LUA_TSTRING,
	  "lua_rawseti accepts negative wider 64-bit C integer key");
    check_string(L, -1, "raw-neg40-seti",
		 "lua_rawseti negative wider 64-bit key value");
    lua_pop(L, 1);
  }

  rtype = lua_rawgetp_sig(L, -1, &pointer_key);
  check(L, rtype == LUA_TSTRING, "lua_rawgetp function pointer return type");
  check_string(L, -1, "ptr-value", "lua_rawgetp return value");
  lua_pop(L, 1);

  rtype = lua_getglobal_sig(L, "debug");
  check(L, rtype == LUA_TTABLE, "lua_getglobal function pointer return type");
  lua_pop(L, 2);
}

static void test_uservalue_api(lua_State *L)
{
  int status;
  int top;
  int udidx;
  void *alias_ud;
  void *ud = lua_newuserdatauv(L, 4, 2);
  check(L, ud != NULL, "lua_newuserdatauv");

  lua_pushcfunction(L, newuserdatauv_negative_nuv);
  status = lua_pcall(L, 0, 0, 0);
  check(L, status == LUA_ERRRUN, "lua_newuserdatauv rejects negative nuvalue");
  check(L, strstr(lua_tostring(L, -1), "invalid value") != NULL,
	"lua_newuserdatauv negative nuvalue error");
  lua_pop(L, 1);

  lua_pushcfunction(L, newuserdatauv_large_nuv);
  status = lua_pcall(L, 0, 0, 0);
  check(L, status == LUA_ERRRUN, "lua_newuserdatauv rejects large nuvalue");
  check(L, strstr(lua_tostring(L, -1), "invalid value") != NULL,
	"lua_newuserdatauv large nuvalue error");
  lua_pop(L, 1);

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
  check(L, lua_gettop(L) == top + 1, "lua_getiuservalue none pushes nil");
  check(L, lua_isnil(L, -1), "lua_getiuservalue out of range nil");
  lua_pop(L, 1);
  check(L, lua_getiuservalue(L, -1, 0) == LUA_TNONE,
	"lua_getiuservalue zero index out of range");
  check(L, lua_isnil(L, -1), "lua_getiuservalue zero index nil");
  lua_pop(L, 1);
  check(L, lua_getiuservalue(L, -1, -1) == LUA_TNONE,
	"lua_getiuservalue negative index out of range");
  check(L, lua_isnil(L, -1), "lua_getiuservalue negative index nil");
  lua_pop(L, 1);
  lua_pushnil(L);
  check(L, lua_setiuservalue(L, -2, 3) == 0,
	"lua_setiuservalue out of range");
  check(L, lua_gettop(L) == top, "lua_setiuservalue invalid pops value");
  lua_newtable(L);
  top = lua_gettop(L);
  check(L, lua_getiuservalue(L, -1, 1) == LUA_TNONE,
	"lua_getiuservalue non-userdata");
  check(L, lua_gettop(L) == top,
	"lua_getiuservalue non-userdata pushes nothing");
  lua_pushliteral(L, "ignored");
  check(L, lua_setiuservalue(L, -2, 1) == 0,
	"lua_setiuservalue non-userdata");
  check(L, lua_gettop(L) == top,
	"lua_setiuservalue non-userdata pops value");
  lua_pop(L, 1);

  alias_ud = lua_newuserdata(L, 4);
  check(L, alias_ud != NULL, "lua_newuserdata alias");
  lua_pushliteral(L, "uv-alias");
  check(L, lua_setuservalue(L, -2) == 1, "lua_setuservalue alias");
  check(L, lua_getuservalue(L, -1) == LUA_TSTRING,
	"lua_getuservalue alias type");
  check_string(L, -1, "uv-alias", "lua_getuservalue alias value");
  lua_pop(L, 2);

  udidx = lua_gettop(L);
  status = luaL_dostring(L, "if jit then jit.off(); jit.flush() end");
  check(L, status == LUA_OK, "userdata GC barrier JIT-off setup");
  lua_gc(L, LUA_GCGEN, 1, 1000);
  lua_gc(L, LUA_GCCOLLECT);
  lua_gc(L, LUA_GCCOLLECT);

  lua_newtable(L);
  lua_newtable(L);
  lua_pushinteger(L, 73);
  lua_setfield(L, -2, "value");
  lua_setfield(L, -2, "child");
  check(L, lua_setiuservalue(L, udidx, 2) == 1,
	"lua_setiuservalue generational young graph");
  lua_gc(L, LUA_GCSTEP, 0);
  lua_gc(L, LUA_GCSTEP, 0);
  check(L, lua_getiuservalue(L, udidx, 2) == LUA_TTABLE,
	"lua_getiuservalue keeps generational graph root");
  lua_getfield(L, -1, "child");
  lua_getfield(L, -1, "value");
  check_integer(L, -1, 73, "lua_setiuservalue keeps generational child");
  lua_pop(L, 3);

  lua_newtable(L);
  lua_newtable(L);
  lua_pushinteger(L, 91);
  lua_setfield(L, -2, "value");
  lua_setfield(L, -2, "child");
  check(L, lua_setmetatable(L, udidx) == 1,
	"lua_setmetatable generational young graph");
  lua_gc(L, LUA_GCSTEP, 0);
  lua_gc(L, LUA_GCSTEP, 0);
  check(L, lua_getmetatable(L, udidx) == 1,
	"lua_getmetatable keeps generational graph root");
  lua_getfield(L, -1, "child");
  lua_getfield(L, -1, "value");
  check_integer(L, -1, 91, "lua_setmetatable keeps generational child");
  lua_pop(L, 3);

  lua_gc(L, LUA_GCINC, 0, 0, 0);
  status = luaL_dostring(L, "if jit then jit.on() end");
  check(L, status == LUA_OK, "userdata GC barrier JIT restore");

  lua_pushcfunction(L, push_plain_userdata);
  lua_setglobal(L, "capi_plain_userdata");
  status = luaL_dostring(L,
    "if jit then jit.off(); jit.flush() end\n"
    "collectgarbage('generational', 1, 1000)\n"
    "local anchor = { false }\n"
    "local seen\n"
    "local function gcf(obj)\n"
    "  anchor[1] = obj\n"
    "  obj = nil\n"
    "  collectgarbage('step', 0)\n"
    "  local mt = getmetatable(anchor[1])\n"
    "  seen = mt and mt.x\n"
    "end\n"
    "collectgarbage('collect')\n"
    "local obj = capi_plain_userdata()\n"
    "collectgarbage('step', 0)\n"
    "debug.setmetatable(obj, { __gc = gcf, x = '+' })\n"
    "obj = nil\n"
    "collectgarbage('step', 0)\n"
    "assert(seen == '+')\n"
    "if jit then jit.on() end\n");
  check(L, status == LUA_OK,
	"generational userdata finalizer keeps metatable graph");

  status = luaL_dostring(L,
    "collectgarbage('incremental')\n"
    "collectgarbage('stop')\n"
    "local function newproxy(u)\n"
    "  local obj = capi_plain_userdata()\n"
    "  if u ~= nil then debug.setmetatable(obj, debug.getmetatable(u)) end\n"
    "  return obj\n"
    "end\n"
    "local u = newproxy(nil)\n"
    "debug.setmetatable(u, { __gc = true })\n"
    "local s = 0\n"
    "local a = setmetatable({ [u] = 0 }, { __mode = 'vk' })\n"
    "for i = 1, 10 do a[newproxy(u)] = i end\n"
    "for k in pairs(a) do assert(getmetatable(k) == getmetatable(u)) end\n"
    "local a1 = {}\n"
    "for k, v in pairs(a) do a1[k] = v end\n"
    "for k, v in pairs(a1) do a[v] = k end\n"
    "for i = 1, 10 do assert(a[i]) end\n"
    "getmetatable(u).a = a1\n"
    "getmetatable(u).u = u\n"
    "do\n"
    "  local u = u\n"
    "  getmetatable(u).__gc = function(o)\n"
    "    assert(a[o] == 10 - s)\n"
    "    assert(a[10 - s] == nil)\n"
    "    assert(getmetatable(o) == getmetatable(u))\n"
    "    assert(getmetatable(o).a[o] == 10 - s)\n"
    "    s = s + 1\n"
    "  end\n"
    "end\n"
    "a1, u = nil\n"
    "assert(next(a) ~= nil)\n"
    "collectgarbage('collect')\n"
    "assert(s == 11)\n"
    "collectgarbage('collect')\n"
    "assert(next(a) == nil)\n"
    "collectgarbage('restart')\n");
  check(L, status == LUA_OK,
	"userdata weak table finalizers clear weak aliases");
  lua_pushnil(L);
  lua_setglobal(L, "capi_plain_userdata");

  lua_getglobal(L, "debug");
  lua_getfield(L, -1, "getuservalue");
  lua_pushvalue(L, -3);
  lua_pushinteger(L, 1);
  lua_call(L, 2, 1);
  check_string(L, -1, "uv1", "debug.getuservalue reads declared slot");
  lua_pop(L, 1);

  lua_getfield(L, -1, "setuservalue");
  lua_pushvalue(L, -3);
  lua_pushliteral(L, "uv2");
  lua_pushinteger(L, 2);
  lua_call(L, 3, 1);
  check(L, lua_touserdata(L, -1) == ud, "debug.setuservalue returns userdata");
  lua_pop(L, 1);

  lua_getfield(L, -1, "getuservalue");
  lua_pushvalue(L, -3);
  lua_pushinteger(L, 2);
  lua_call(L, 2, 1);
  check_string(L, -1, "uv2", "debug.setuservalue updates declared slot");
  lua_pop(L, 1);

  lua_getfield(L, -1, "setuservalue");
  lua_pushvalue(L, -3);
  lua_pushliteral(L, "bad");
  lua_pushinteger(L, 3);
  lua_call(L, 3, 1);
  check(L, lua_isnil(L, -1), "debug.setuservalue rejects undeclared slot");
  lua_pop(L, 2);
}

static void test_metatable_api54(lua_State *L)
{
  int top = lua_gettop(L);

  lua_newtable(L);
  check(L, lua_getmetatable(L, -1) == 0, "lua_getmetatable no metatable");
  check(L, lua_gettop(L) == top + 1,
	"lua_getmetatable no metatable pushes nothing");

  lua_newtable(L);
  lua_pushliteral(L, "lua54-mt");
  lua_setfield(L, -2, "tag");
  check(L, lua_setmetatable(L, -2) == 1, "lua_setmetatable table return");
  check(L, lua_gettop(L) == top + 1, "lua_setmetatable pops metatable");
  check(L, lua_getmetatable(L, -1) == 1, "lua_getmetatable table return");
  lua_getfield(L, -1, "tag");
  check_string(L, -1, "lua54-mt", "lua_getmetatable table value");
  lua_pop(L, 2);

  lua_pushnil(L);
  check(L, lua_setmetatable(L, -2) == 1, "lua_setmetatable nil return");
  check(L, lua_gettop(L) == top + 1,
	"lua_setmetatable nil still pops value");
  check(L, lua_getmetatable(L, -1) == 0, "lua_setmetatable nil removes mt");
  lua_pop(L, 1);

  check(L, lua_gettop(L) == top, "metatable api restores stack");
}

static void test_upvalue_api54(lua_State *L)
{
  int top = lua_gettop(L);
  int status;
  int f1, f2;
  const char *name;
  void *id1;
  void *id2;

  status = luaL_loadstring(L,
    "local x = 'left'\n"
    "local y = 'right'\n"
    "return function() return x end, function() return y end");
  check(L, status == LUA_OK, "load upvalue api probe");
  lua_call(L, 0, 2);
  f1 = lua_absindex(L, -2);
  f2 = lua_absindex(L, -1);

  name = lua_getupvalue(L, f1, 1);
  check(L, name && strcmp(name, "x") == 0, "lua_getupvalue name");
  check_string(L, -1, "left", "lua_getupvalue value");
  lua_pop(L, 1);
  check(L, lua_getupvalue(L, f1, 2) == NULL,
	"lua_getupvalue invalid index");
  check(L, lua_gettop(L) == top + 2,
	"lua_getupvalue invalid pushes nothing");
  lua_pushboolean(L, 1);
  check(L, lua_getupvalue(L, -1, 1) == NULL,
	"lua_getupvalue non-function returns NULL");
  check(L, lua_gettop(L) == top + 3,
	"lua_getupvalue non-function pushes nothing");
  lua_pushnil(L);
  check(L, lua_setupvalue(L, -2, 1) == NULL,
	"lua_setupvalue non-function returns NULL");
  check(L, lua_gettop(L) == top + 4,
	"lua_setupvalue non-function preserves value");
  lua_pop(L, 2);

  id1 = lua_upvalueid(L, f1, 1);
  id2 = lua_upvalueid(L, f2, 1);
  check(L, id1 != NULL && id2 != NULL && id1 != id2,
	"lua_upvalueid distinct closures");
  check(L, lua_upvalueid(L, f1, 2) == NULL,
	"lua_upvalueid invalid index");

  lua_pushliteral(L, "changed");
  name = lua_setupvalue(L, f1, 1);
  check(L, name && strcmp(name, "x") == 0, "lua_setupvalue name");
  check(L, lua_gettop(L) == top + 2, "lua_setupvalue pops value");
  lua_pushvalue(L, f1);
  lua_call(L, 0, 1);
  check_string(L, -1, "changed", "lua_setupvalue updates closure");
  lua_pop(L, 1);

  lua_upvaluejoin(L, f1, 1, f2, 1);
  check(L, lua_upvalueid(L, f1, 1) == lua_upvalueid(L, f2, 1),
	"lua_upvaluejoin shared id");
  lua_pushvalue(L, f1);
  lua_call(L, 0, 1);
  check_string(L, -1, "right", "lua_upvaluejoin adopts source value");
  lua_pop(L, 1);

  lua_pushliteral(L, "joined");
  name = lua_setupvalue(L, f2, 1);
  check(L, name && strcmp(name, "y") == 0, "lua_setupvalue source name");
  lua_pushvalue(L, f1);
  lua_call(L, 0, 1);
  check_string(L, -1, "joined", "lua_upvaluejoin keeps shared slot");
  lua_pop(L, 1);

  lua_pop(L, 2);

  status = luaL_loadstring(L,
    "local x = 'old'\n"
    "return function() return x end");
  check(L, status == LUA_OK, "load real upvalue target");
  lua_call(L, 0, 1);
  f1 = lua_absindex(L, -1);
  status = luaL_loadstring(L, "return capi_fake_env_value");
  check(L, status == LUA_OK, "load fake _ENV source");
  f2 = lua_absindex(L, -1);
  lua_upvaluejoin(L, f1, 1, f2, 1);
  lua_pushvalue(L, f1);
  lua_call(L, 0, 1);
  lua_pushglobaltable(L);
  check(L, lua_rawequal(L, -1, -2),
	"lua_upvaluejoin adopts fake _ENV source");
  lua_pop(L, 4);

  check(L, lua_gettop(L) == top, "upvalue api restores stack");
}

static void test_lauxlib_buffer_too_large(lua_State *L)
{
  FakeHugeAllocCtx ctx;
  lua_State *T;
  int status;
  memset(&ctx, 0, sizeof(ctx));
  T = luaL_newstate();
  check(L, T != NULL, "luaL_Buffer overflow state");
  ctx.oldf = lua_getallocf(T, &ctx.oldud);
  ctx.fake_threshold = ~(size_t)0 / 2u;
  lua_setallocf(T, fake_huge_alloc, &ctx);
  lua_pushcfunction(T, overflow_laux_buffer);
  status = lua_pcall(T, 0, 0, 0);
  check(L, status == LUA_ERRRUN, "luaL_Buffer overflow status");
  check(L, lua_tostring(T, -1) != NULL &&
	   strstr(lua_tostring(T, -1), "buffer too large") != NULL,
	"luaL_Buffer overflow error");
  lua_pop(T, 1);
  lua_close(T);
  check(L, ctx.fake_allocs == 1, "luaL_Buffer overflow fake allocation");
  check(L, ctx.fake_frees == 1, "luaL_Buffer overflow closes fake box");
  check(L, ctx.fake_reallocs == 0, "luaL_Buffer overflow avoids fake grow");
  check(L, ctx.fake_size >= ctx.fake_threshold,
	"luaL_Buffer overflow exercised huge allocation");
}

static void test_lauxlib_api(lua_State *L)
{
  luaL_Buffer b;
  luaL_Stream stream;
  char *p;
  void (*pushresultsize_fn)(luaL_Buffer *, size_t) = luaL_pushresultsize;
  char *(*buffinitsize_fn)(lua_State *, luaL_Buffer *, size_t) =
    luaL_buffinitsize;
  const char *gs;
  int status;
  int ref;
  int rtype;
  int before_count;
  void *ud;
  FILE *tmpf;
  const char *tmpname = "test/lua54_capi_dofile.tmp.lua";
  const char *binname = "test/lua54_capi_hash_binary.tmp";
  CApiReaderCtx reader;
  DumpBuffer dump;

  stream.f = NULL;
  stream.closef = NULL;
  check(L, stream.f == NULL && stream.closef == NULL, "luaL_Stream fields");

  luaL_checkversion(L);
  luaL_argexpected(L, 1, 1, "truthy condition");

  lua_pushcfunction(L, checkversion_bad_version);
  status = lua_pcall(L, 0, 0, 0);
  check(L, status == LUA_ERRRUN, "luaL_checkversion_ rejects wrong version");
  check(L, strstr(lua_tostring(L, -1), "version mismatch") != NULL,
	"luaL_checkversion_ version error");
  check(L, strstr(lua_tostring(L, -1), "503.0") != NULL &&
	   strstr(lua_tostring(L, -1), "504.0") != NULL,
	"luaL_checkversion_ version numbers are Lua numbers");
  lua_pop(L, 1);

  lua_pushcfunction(L, checkversion_bad_sizes);
  status = lua_pcall(L, 0, 0, 0);
  check(L, status == LUA_ERRRUN, "luaL_checkversion_ rejects wrong sizes");
  check(L, strstr(lua_tostring(L, -1), "incompatible numeric types") != NULL,
	"luaL_checkversion_ sizes error");
  lua_pop(L, 1);

  lua_pushcfunction(L, checkinteger_fraction);
  lua_pushnumber(L, (lua_Number)1.5);
  status = lua_pcall(L, 1, 0, 0);
  check(L, status == LUA_ERRRUN, "luaL_checkinteger rejects fraction");
  check(L, strstr(lua_tostring(L, -1),
		  "number has no integer representation") != NULL,
	"luaL_checkinteger fraction error");
  lua_pop(L, 1);

  lua_pushcfunction(L, optinteger_fraction);
  lua_pushnumber(L, (lua_Number)1.5);
  status = lua_pcall(L, 1, 0, 0);
  check(L, status == LUA_ERRRUN, "luaL_optinteger rejects fraction");
  check(L, strstr(lua_tostring(L, -1),
		  "number has no integer representation") != NULL,
	"luaL_optinteger fraction error");
  lua_pop(L, 1);

  lua_pushcfunction(L, checkinteger_arg);
  lua_pushliteral(L, "inf");
  status = lua_pcall(L, 1, 0, 0);
  check(L, status == LUA_ERRRUN, "luaL_checkinteger rejects inf string");
  check(L, strstr(lua_tostring(L, -1), "number expected") != NULL,
	"luaL_checkinteger inf error");
  lua_pop(L, 1);

  lua_pushcfunction(L, optinteger_arg);
  lua_pushliteral(L, "0b10");
  status = lua_pcall(L, 1, 0, 0);
  check(L, status == LUA_ERRRUN,
	"luaL_optinteger rejects binary prefix string");
  check(L, strstr(lua_tostring(L, -1), "number expected") != NULL,
	"luaL_optinteger binary prefix error");
  lua_pop(L, 1);

  if (sizeof(lua_Integer) > sizeof(int)) {
    lua_Integer big = (lua_Integer)((lua_Unsigned)0x7fffffffu + 1u);
    lua_Integer big40 = (lua_Integer)1024 * 1024 * 1024 * 1024;
    lua_Number pos63 = -(lua_Number)LUA_MININTEGER;
    lua_pushcfunction(L, checkinteger_arg);
    lua_pushnumber(L, (lua_Number)2147483648.0);
    status = lua_pcall(L, 1, 1, 0);
    check(L, status == LUA_OK, "luaL_checkinteger accepts 64-bit number");
    check_integer(L, -1, big, "luaL_checkinteger 64-bit number");
    lua_pop(L, 1);
    lua_pushcfunction(L, checkinteger_arg);
    lua_pushnumber(L, (lua_Number)big40);
    status = lua_pcall(L, 1, 1, 0);
    check(L, status == LUA_OK,
	  "luaL_checkinteger accepts wider 64-bit number");
    check_integer(L, -1, big40, "luaL_checkinteger wider 64-bit number");
    lua_pop(L, 1);
    lua_pushcfunction(L, checkinteger_arg);
    lua_pushliteral(L, "1099511627776");
    status = lua_pcall(L, 1, 1, 0);
    check(L, status == LUA_OK,
	  "luaL_checkinteger accepts wider 64-bit string");
    check_integer(L, -1, big40, "luaL_checkinteger wider 64-bit string");
    lua_pop(L, 1);
    lua_pushcfunction(L, checkinteger_arg);
    lua_pushliteral(L, "0x8000000000000000");
    status = lua_pcall(L, 1, 1, 0);
    check(L, status == LUA_OK,
	  "luaL_checkinteger accepts hex LUA_MININTEGER");
    check_integer(L, -1, LUA_MININTEGER,
		  "luaL_checkinteger hex LUA_MININTEGER");
    lua_pop(L, 1);
    lua_pushcfunction(L, checkinteger_arg);
    lua_pushliteral(L, "0xffffffffffffffff");
    status = lua_pcall(L, 1, 1, 0);
    check(L, status == LUA_OK,
	  "luaL_checkinteger accepts hex unsigned minus one");
    check_integer(L, -1, (lua_Integer)-1,
		  "luaL_checkinteger hex unsigned minus one");
    lua_pop(L, 1);
    lua_pushcfunction(L, checkinteger_arg);
    lua_pushliteral(L, "0x10000000000000000");
    status = lua_pcall(L, 1, 1, 0);
    check(L, status == LUA_OK,
	  "luaL_checkinteger accepts hex unsigned wrap");
    check_integer(L, -1, 0, "luaL_checkinteger hex unsigned wrap");
    lua_pop(L, 1);
    lua_pushcfunction(L, checkinteger_arg);
    lua_pushliteral(L, "9223372036854775808");
    status = lua_pcall(L, 1, 0, 0);
    check(L, status == LUA_ERRRUN,
	  "luaL_checkinteger rejects string above LUA_MAXINTEGER");
    check(L, strstr(lua_tostring(L, -1),
		    "number has no integer representation") != NULL,
	  "luaL_checkinteger above max error");
    lua_pop(L, 1);
    lua_pushcfunction(L, optinteger_arg);
    lua_pushnumber(L, (lua_Number)big40);
    status = lua_pcall(L, 1, 1, 0);
    check(L, status == LUA_OK,
	  "luaL_optinteger accepts wider 64-bit number");
    check_integer(L, -1, big40, "luaL_optinteger wider 64-bit number");
    lua_pop(L, 1);
    lua_pushcfunction(L, optinteger_arg);
    lua_pushliteral(L, "-9223372036854775809");
    status = lua_pcall(L, 1, 1, 0);
    check(L, status == LUA_OK,
	  "luaL_optinteger accepts rounded string below LUA_MININTEGER");
    check_integer(L, -1, LUA_MININTEGER,
		  "luaL_optinteger rounded below LUA_MININTEGER");
    lua_pop(L, 1);
    lua_pushcfunction(L, optinteger_arg);
    lua_pushliteral(L, "0x10000000000000000");
    status = lua_pcall(L, 1, 1, 0);
    check(L, status == LUA_OK,
	  "luaL_optinteger accepts hex unsigned wrap");
    check_integer(L, -1, 0, "luaL_optinteger hex unsigned wrap");
    lua_pop(L, 1);
    lua_pushcfunction(L, optinteger_arg);
    lua_pushliteral(L, "9223372036854775808");
    status = lua_pcall(L, 1, 0, 0);
    check(L, status == LUA_ERRRUN,
	  "luaL_optinteger rejects string above LUA_MAXINTEGER");
    check(L, strstr(lua_tostring(L, -1),
		    "number has no integer representation") != NULL,
	  "luaL_optinteger above max error");
    lua_pop(L, 1);
    lua_pushcfunction(L, optinteger_arg);
    status = lua_pcall(L, 0, 1, 0);
    check(L, status == LUA_OK, "luaL_optinteger wider default status");
    check_integer(L, -1, (lua_Integer)2048 * 1024 * 1024,
		  "luaL_optinteger wider default");
    lua_pop(L, 1);
    lua_pushcfunction(L, checknumber_arg);
    lua_pushinteger(L, big40);
    status = lua_pcall(L, 1, 1, 0);
    check(L, status == LUA_OK,
	  "luaL_checknumber accepts boxed 64-bit integer");
    check(L, lua_tonumber(L, -1) == (lua_Number)big40,
	  "luaL_checknumber boxed 64-bit integer");
    lua_pop(L, 1);
    lua_pushcfunction(L, checknumber_arg);
    lua_pushliteral(L, "1099511627776");
    status = lua_pcall(L, 1, 1, 0);
    check(L, status == LUA_OK,
	  "luaL_checknumber accepts wider 64-bit string number");
    check(L, lua_tonumber(L, -1) == (lua_Number)big40,
	  "luaL_checknumber wider 64-bit string number");
    lua_pop(L, 1);
    lua_pushcfunction(L, checknumber_arg);
    lua_pushliteral(L, "9223372036854775808");
    status = lua_pcall(L, 1, 1, 0);
    check(L, status == LUA_OK,
	  "luaL_checknumber accepts decimal above LUA_MAXINTEGER");
    check(L, !lua_isinteger(L, -1) && lua_tonumber(L, -1) == pos63,
	  "luaL_checknumber decimal above LUA_MAXINTEGER");
    lua_pop(L, 1);
    lua_pushcfunction(L, checknumber_arg);
    lua_pushliteral(L, "-9223372036854775809");
    status = lua_pcall(L, 1, 1, 0);
    check(L, status == LUA_OK,
	  "luaL_checknumber accepts decimal below LUA_MININTEGER");
    check(L, !lua_isinteger(L, -1) && lua_tonumber(L, -1) == -pos63,
	  "luaL_checknumber decimal below LUA_MININTEGER");
    lua_pop(L, 1);
    lua_pushcfunction(L, optnumber_arg);
    lua_pushinteger(L, big40);
    status = lua_pcall(L, 1, 1, 0);
    check(L, status == LUA_OK,
	  "luaL_optnumber accepts boxed 64-bit integer");
    check(L, lua_tonumber(L, -1) == (lua_Number)big40,
	  "luaL_optnumber boxed 64-bit integer");
    lua_pop(L, 1);
    lua_pushcfunction(L, optnumber_arg);
    lua_pushliteral(L, "1099511627776");
    status = lua_pcall(L, 1, 1, 0);
    check(L, status == LUA_OK,
	  "luaL_optnumber accepts wider 64-bit string number");
    check(L, lua_tonumber(L, -1) == (lua_Number)big40,
	  "luaL_optnumber wider 64-bit string number");
    lua_pop(L, 1);
    lua_pushcfunction(L, optnumber_arg);
    lua_pushliteral(L, "18446744073709551616");
    status = lua_pcall(L, 1, 1, 0);
    check(L, status == LUA_OK,
	  "luaL_optnumber accepts decimal unsigned range");
    check(L, !lua_isinteger(L, -1) && lua_tonumber(L, -1) == pos63 * 2.0,
	  "luaL_optnumber decimal unsigned range");
    lua_pop(L, 1);
  }

  lua_pushcfunction(L, optnumber_arg);
  status = lua_pcall(L, 0, 1, 0);
  check(L, status == LUA_OK, "luaL_optnumber default status");
  check(L, lua_tonumber(L, -1) == (lua_Number)3.25,
	"luaL_optnumber default");
  lua_pop(L, 1);

  lua_pushcfunction(L, optnumber_arg);
  lua_pushliteral(L, "4.5");
  status = lua_pcall(L, 1, 1, 0);
  check(L, status == LUA_OK, "luaL_optnumber string status");
  check(L, lua_tonumber(L, -1) == (lua_Number)4.5,
	"luaL_optnumber string");
  lua_pop(L, 1);

  lua_pushcfunction(L, optnumber_arg);
  lua_pushliteral(L, "inf");
  status = lua_pcall(L, 1, 0, 0);
  check(L, status == LUA_ERRRUN, "luaL_optnumber rejects inf string");
  check(L, strstr(lua_tostring(L, -1), "number expected") != NULL,
	"luaL_optnumber inf error");
  lua_pop(L, 1);

  lua_pushcfunction(L, optnumber_arg);
  lua_pushliteral(L, "nan");
  status = lua_pcall(L, 1, 0, 0);
  check(L, status == LUA_ERRRUN, "luaL_optnumber rejects nan string");
  check(L, strstr(lua_tostring(L, -1), "number expected") != NULL,
	"luaL_optnumber nan error");
  lua_pop(L, 1);

  lua_pushcfunction(L, checknumber_arg);
  lua_pushliteral(L, "inf");
  status = lua_pcall(L, 1, 0, 0);
  check(L, status == LUA_ERRRUN, "luaL_checknumber rejects inf string");
  check(L, strstr(lua_tostring(L, -1), "number expected") != NULL,
	"luaL_checknumber inf error");
  lua_pop(L, 1);

  lua_pushcfunction(L, checknumber_arg);
  lua_pushliteral(L, "nan");
  status = lua_pcall(L, 1, 0, 0);
  check(L, status == LUA_ERRRUN, "luaL_checknumber rejects nan string");
  check(L, strstr(lua_tostring(L, -1), "number expected") != NULL,
	"luaL_checknumber nan error");
  lua_pop(L, 1);

  lua_pushcfunction(L, checknumber_arg);
  lua_pushliteral(L, "0b10");
  status = lua_pcall(L, 1, 0, 0);
  check(L, status == LUA_ERRRUN,
	"luaL_checknumber rejects binary prefix string");
  check(L, strstr(lua_tostring(L, -1), "number expected") != NULL,
	"luaL_checknumber binary prefix error");
  lua_pop(L, 1);

  luaL_pushfail(L);
  check(L, lua_isnil(L, -1), "luaL_pushfail pushes nil");
  lua_pop(L, 1);

  lua_pushcfunction(L, checkoption_arg);
  status = lua_pcall(L, 0, 1, 0);
  check(L, status == LUA_OK, "luaL_checkoption default status");
  check_integer(L, -1, 1, "luaL_checkoption default index");
  lua_pop(L, 1);

  lua_pushcfunction(L, checkoption_arg);
  lua_pushnil(L);
  status = lua_pcall(L, 1, 1, 0);
  check(L, status == LUA_OK, "luaL_checkoption nil default status");
  check_integer(L, -1, 1, "luaL_checkoption nil default index");
  lua_pop(L, 1);

  lua_pushcfunction(L, checkoption_arg);
  lua_pushliteral(L, "gamma");
  status = lua_pcall(L, 1, 1, 0);
  check(L, status == LUA_OK, "luaL_checkoption explicit status");
  check_integer(L, -1, 2, "luaL_checkoption explicit index");
  lua_pop(L, 1);

  lua_pushcfunction(L, checkoption_arg);
  lua_pushliteral(L, "delta");
  status = lua_pcall(L, 1, 0, 0);
  check(L, status == LUA_ERRRUN, "luaL_checkoption rejects option");
  check(L, strstr(lua_tostring(L, -1), "invalid option") != NULL,
	"luaL_checkoption error");
  lua_pop(L, 1);

  lua_pushcfunction(L, checkoption_arg);
  lua_newtable(L);
  status = lua_pcall(L, 1, 0, 0);
  check(L, status == LUA_ERRRUN, "luaL_checkoption rejects table with default");
  check(L, strstr(lua_tostring(L, -1), "string expected") != NULL,
	"luaL_checkoption table error");
  lua_pop(L, 1);
  check_fresh_invalid_value(L, checkoption_null_list,
			    "luaL_checkoption rejects NULL option list",
			    "luaL_checkoption NULL option list error");

  lua_pushcfunction(L, laux_string_macro_arg);
  lua_pushliteral(L, "left");
  status = lua_pcall(L, 1, 1, 0);
  check(L, status == LUA_OK, "luaL_checkstring/luaL_optstring default status");
  check_string(L, -1, "left/fallback",
	       "luaL_checkstring/luaL_optstring default");
  lua_pop(L, 1);

  lua_pushcfunction(L, laux_string_macro_arg);
  lua_pushliteral(L, "left");
  lua_pushliteral(L, "right");
  status = lua_pcall(L, 2, 1, 0);
  check(L, status == LUA_OK, "luaL_optstring explicit status");
  check_string(L, -1, "left/right", "luaL_optstring explicit");
  lua_pop(L, 1);

  lua_pushcfunction(L, laux_string_macro_arg);
  status = lua_pcall(L, 0, 0, 0);
  check(L, status == LUA_ERRRUN, "luaL_checkstring rejects missing arg");
  check(L, strstr(lua_tostring(L, -1), "string expected") != NULL,
	"luaL_checkstring missing arg error");
  lua_pop(L, 1);

  lua_pushcfunction(L, laux_argcheck_fail);
  status = lua_pcall(L, 0, 0, 0);
  check(L, status == LUA_ERRRUN, "luaL_argcheck macro rejects");
  check(L, strstr(lua_tostring(L, -1), "macro guard failed") != NULL,
	"luaL_argcheck macro error");
  lua_pop(L, 1);

  lua_pushcfunction(L, laux_argexpected_fail);
  status = lua_pcall(L, 0, 0, 0);
  check(L, status == LUA_ERRRUN, "luaL_argexpected macro rejects");
  check(L, strstr(lua_tostring(L, -1), "macro-value expected") != NULL,
	"luaL_argexpected macro error");
  lua_pop(L, 1);

  lua_pushcfunction(L, laux_argexpected_table_arg);
  lua_pushlightuserdata(L, (void *)&status);
  status = lua_pcall(L, 1, 0, 0);
  check(L, status == LUA_ERRRUN,
	"luaL_argexpected light userdata status");
  check(L, strstr(lua_tostring(L, -1),
		  "table expected, got light userdata") != NULL,
	"luaL_argexpected light userdata name");
  lua_pop(L, 1);

  lua_pushcfunction(L, laux_argerror_fail);
  lua_setglobal(L, "capi_argerror_fail");
  status = luaL_loadstring(L, "capi_argerror_fail(false)");
  check(L, status == LUA_OK, "luaL_argerror source global load");
  status = lua_pcall(L, 0, 0, 0);
  check(L, status == LUA_ERRRUN, "luaL_argerror source global status");
  check(L, strstr(lua_tostring(L, -1),
		  "bad argument #1 to 'capi_argerror_fail'") != NULL &&
	   strstr(lua_tostring(L, -1), "explicit laux failure") != NULL,
	"luaL_argerror source global call name");
  lua_pop(L, 1);

  lua_pushcfunction(L, laux_argerror_negative_arg);
  lua_setglobal(L, "capi_argerror_negative_arg");
  status = luaL_loadstring(L, "capi_argerror_negative_arg('a', 'b')");
  check(L, status == LUA_OK, "luaL_argerror negative index load");
  status = lua_pcall(L, 0, 0, 0);
  check(L, status == LUA_ERRRUN, "luaL_argerror negative index status");
  check(L, strstr(lua_tostring(L, -1),
		  "bad argument #2 to 'capi_argerror_negative_arg'") != NULL &&
	   strstr(lua_tostring(L, -1), "negative index failure") != NULL,
	"luaL_argerror negative index normalization");
  lua_pop(L, 1);

  status = luaL_loadstring(L,
    "local obj = { f = capi_argerror_fail }\n"
    "obj:f()\n");
  check(L, status == LUA_OK, "luaL_argerror method load");
  status = lua_pcall(L, 0, 0, 0);
  check(L, status == LUA_ERRRUN, "luaL_argerror method status");
  check(L, strstr(lua_tostring(L, -1),
		  "calling 'f' on bad self") != NULL &&
	   strstr(lua_tostring(L, -1), "explicit laux failure") != NULL,
	"luaL_argerror method bad self text");
  lua_pop(L, 1);
  lua_pushnil(L);
  lua_setglobal(L, "capi_argerror_fail");
  lua_pushnil(L);
  lua_setglobal(L, "capi_argerror_negative_arg");

  lua_pushcfunction(L, laux_checktype_any_arg);
  lua_newtable(L);
  status = lua_pcall(L, 1, 1, 0);
  check(L, status == LUA_OK, "luaL_checkany/luaL_checktype table status");
  check_string(L, -1, "ok", "luaL_checkany/luaL_checktype table");
  lua_pop(L, 1);

  lua_pushcfunction(L, laux_checktype_any_arg);
  status = lua_pcall(L, 0, 0, 0);
  check(L, status == LUA_ERRRUN, "luaL_checkany rejects missing arg");
  check(L, strstr(lua_tostring(L, -1), "value expected") != NULL,
	"luaL_checkany missing arg error");
  lua_pop(L, 1);

  lua_pushcfunction(L, laux_checktype_any_arg);
  lua_pushliteral(L, "not-table");
  status = lua_pcall(L, 1, 0, 0);
  check(L, status == LUA_ERRRUN, "luaL_checktype rejects wrong type");
  check(L, strstr(lua_tostring(L, -1), "table expected") != NULL,
	"luaL_checktype wrong type error");
  lua_pop(L, 1);

  check_fresh_invalid_value(L, laux_checktype_invalid_expected_low,
			    "luaL_checktype rejects below LUA_TNONE",
			    "luaL_checktype below LUA_TNONE error");
  check_fresh_invalid_value(L, laux_checktype_invalid_expected_high,
			    "luaL_checktype rejects too-large expected type",
			    "luaL_checktype too-large expected type error");

  lua_pushcfunction(L, laux_checktype_any_arg);
  lua_pushlightuserdata(L, (void *)&status);
  status = lua_pcall(L, 1, 0, 0);
  check(L, status == LUA_ERRRUN,
	"luaL_typeerror light userdata status");
  check(L, strstr(lua_tostring(L, -1),
		  "table expected, got light userdata") != NULL,
	"luaL_typeerror light userdata name");
  lua_pop(L, 1);

  lua_pushcfunction(L, laux_checkthread_arg);
  lua_newtable(L);
  lua_newtable(L);
  lua_pushinteger(L, 123);
  lua_setfield(L, -2, "__name");
  lua_setmetatable(L, -2);
  status = lua_pcall(L, 1, 0, 0);
  check(L, status == LUA_ERRRUN, "luaL_typeerror ignores numeric __name");
  check(L, strstr(lua_tostring(L, -1), "thread expected, got table") != NULL,
	"luaL_typeerror numeric __name fallback");
  lua_pop(L, 1);

  lua_pushcfunction(L, checkudata_arg);
  lua_pushinteger(L, 54);
  status = lua_pcall(L, 1, 0, 0);
  check(L, status == LUA_ERRRUN, "luaL_checkudata rejects number");
  check(L, strstr(lua_tostring(L, -1),
		  "capi.ud expected, got number") != NULL,
	"luaL_checkudata number typeerror");
  lua_pop(L, 1);

  lua_pushcfunction(L, checkudata_arg);
  lua_pushlightuserdata(L, (void *)&status);
  status = lua_pcall(L, 1, 0, 0);
  check(L, status == LUA_ERRRUN,
	"luaL_checkudata rejects light userdata");
  check(L, strstr(lua_tostring(L, -1),
		  "capi.ud expected, got light userdata") != NULL,
	"luaL_checkudata light userdata typeerror");
  lua_pop(L, 1);

  lua_pushcfunction(L, checkudata_arg);
  lua_newtable(L);
  lua_newtable(L);
  lua_pushliteral(L, "NamedCapiArg");
  lua_setfield(L, -2, "__name");
  lua_setmetatable(L, -2);
  status = lua_pcall(L, 1, 0, 0);
  check(L, status == LUA_ERRRUN, "luaL_checkudata rejects named table");
  check(L, strstr(lua_tostring(L, -1),
		  "capi.ud expected, got NamedCapiArg") != NULL,
	"luaL_checkudata __name typeerror");
  lua_pop(L, 1);

  lua_pushcfunction(L, checkudata_arg);
  lua_setglobal(L, "capi_checkudata_arg");
  status = luaL_loadstring(L, "capi_checkudata_arg(54)");
  check(L, status == LUA_OK, "luaL_checkudata source global load");
  status = lua_pcall(L, 0, 0, 0);
  check(L, status == LUA_ERRRUN, "luaL_checkudata source global rejects");
  check(L, strstr(lua_tostring(L, -1), "to 'capi_checkudata_arg'") != NULL &&
	   strstr(lua_tostring(L, -1),
		  "capi.ud expected, got number") != NULL,
	"luaL_checkudata source global call name");
  lua_pop(L, 1);

  status = luaL_loadstring(L,
    "local f = capi_checkudata_arg; f(54)");
  check(L, status == LUA_OK, "luaL_checkudata source alias load");
  status = lua_pcall(L, 0, 0, 0);
  check(L, status == LUA_ERRRUN, "luaL_checkudata source alias rejects");
  check(L, strstr(lua_tostring(L, -1), "to 'f'") != NULL &&
	   strstr(lua_tostring(L, -1),
		  "capi.ud expected, got number") != NULL,
	"luaL_checkudata source alias call name");
  lua_pop(L, 1);
  lua_pushnil(L);
  lua_setglobal(L, "capi_checkudata_arg");

  lua_pushcfunction(L, laux_opt_macro_arg);
  status = lua_pcall(L, 0, 1, 0);
  check(L, status == LUA_OK, "luaL_opt macro default status");
  check_integer(L, -1, 77, "luaL_opt macro default");
  lua_pop(L, 1);

  lua_pushcfunction(L, laux_opt_macro_arg);
  lua_pushinteger(L, 54);
  status = lua_pcall(L, 1, 1, 0);
  check(L, status == LUA_OK, "luaL_opt macro explicit status");
  check_integer(L, -1, 54, "luaL_opt macro explicit");
  lua_pop(L, 1);

  lua_pushcfunction(L, laux_checkstack_arg);
  status = lua_pcall(L, 0, 1, 0);
  check(L, status == LUA_OK, "luaL_checkstack status");
  check_string(L, -1, "ok", "luaL_checkstack result");
  lua_pop(L, 1);

  lua_pushcfunction(L, laux_checkstack_null_msg);
  status = lua_pcall(L, 0, 0, 0);
  check(L, status == LUA_ERRRUN, "luaL_checkstack NULL message status");
  check(L, strstr(lua_tostring(L, -1), "stack overflow") != NULL &&
	   strstr(lua_tostring(L, -1), "(null)") == NULL,
	"luaL_checkstack NULL message text");
  lua_pop(L, 1);

  lua_pushcfunction(L, laux_len_arg);
  lua_pushliteral(L, "abcd");
  status = lua_pcall(L, 1, 1, 0);
  check(L, status == LUA_OK, "luaL_len string status");
  check_integer(L, -1, 4, "luaL_len string");
  lua_pop(L, 1);

  lua_pushcfunction(L, laux_len_arg);
  lua_newtable(L);
  lua_newtable(L);
  lua_pushcfunction(L, len_meta);
  lua_setfield(L, -2, "__len");
  lua_setmetatable(L, -2);
  status = lua_pcall(L, 1, 1, 0);
  check(L, status == LUA_OK, "luaL_len __len status");
  check_integer(L, -1, 77, "luaL_len __len");
  lua_pop(L, 1);
  if (sizeof(lua_Integer) > sizeof(int)) {
    lua_pushcfunction(L, laux_len_arg);
    lua_newtable(L);
    lua_newtable(L);
    lua_pushcfunction(L, len_wide_meta);
    lua_setfield(L, -2, "__len");
    lua_setmetatable(L, -2);
    status = lua_pcall(L, 1, 1, 0);
    check(L, status == LUA_OK, "luaL_len wide __len status");
    check_integer(L, -1, (lua_Integer)1024 * 1024 * 1024 * 1024,
		  "luaL_len wide __len");
    lua_pop(L, 1);
  }

  lua_pushcfunction(L, laux_len_arg);
  lua_newtable(L);
  lua_newtable(L);
  lua_pushcfunction(L, len_bad_meta);
  lua_setfield(L, -2, "__len");
  lua_setmetatable(L, -2);
  status = lua_pcall(L, 1, 0, 0);
  check(L, status == LUA_ERRRUN, "luaL_len rejects non-integer length");
  check(L, strstr(lua_tostring(L, -1), "object length is not an integer") != NULL,
	"luaL_len non-integer error");
  lua_pop(L, 1);

  lua_pushcfunction(L, push_fraction_len_userdata);
  lua_setglobal(L, "capi_fraction_len_userdata");
  status = luaL_dostring(L,
    "return table.unpack(capi_fraction_len_userdata())");
  check(L, status != LUA_OK,
	"table.unpack rejects non-table fractional __len");
  check(L, strstr(lua_tostring(L, -1), "object length is not an integer") != NULL,
	"table.unpack fractional __len error");
  lua_pop(L, 1);

  check(L, luaL_intop(+, LUA_MAXINTEGER, 1) == LUA_MININTEGER,
	"luaL_intop add wrap");
  check(L, luaL_intop(-, LUA_MININTEGER, 1) == LUA_MAXINTEGER,
	"luaL_intop sub wrap");
  check(L, luaL_intop(&, (lua_Integer)0x33, (lua_Integer)0x55) == 0x11,
	"luaL_intop bit and");

  lua_newtable(L);
  check(L, luaL_getsubtable(L, -1, "child") == 0,
	"luaL_getsubtable creates");
  check(L, lua_istable(L, -1), "luaL_getsubtable created table");
  lua_pop(L, 1);
  check(L, luaL_getsubtable(L, -1, "child") == 1,
	"luaL_getsubtable reuses");
  lua_pop(L, 2);

  lua_newtable(L);
  lua_newtable(L);
  lua_pushcfunction(L, getsubtable_index_meta);
  lua_setfield(L, -2, "__index");
  lua_setmetatable(L, -2);
  check(L, luaL_getsubtable(L, -1, "virtual") == 1,
	"luaL_getsubtable accepts __index table result");
  lua_getfield(L, -1, "origin");
  check_string(L, -1, "from-index", "luaL_getsubtable __index value");
  lua_pop(L, 1);
  lua_pushliteral(L, "virtual");
  lua_rawget(L, -3);
  check(L, lua_isnil(L, -1), "luaL_getsubtable __index does not raw set");
  lua_pop(L, 3);

  lua_newtable(L);
  lua_newtable(L);
  lua_newtable(L);
  lua_pushvalue(L, -3);
  lua_pushcclosure(L, getsubtable_newindex_meta, 1);
  lua_setfield(L, -2, "__newindex");
  lua_setmetatable(L, -2);
  check(L, luaL_getsubtable(L, -1, "created") == 0,
	"luaL_getsubtable creates through __newindex");
  lua_getfield(L, -3, "captured");
  check(L, lua_rawequal(L, -1, -2),
	"luaL_getsubtable passes new table to __newindex");
  lua_pop(L, 1);
  lua_getfield(L, -3, "key");
  check_string(L, -1, "created", "luaL_getsubtable __newindex key");
  lua_pop(L, 1);
  lua_pushliteral(L, "created");
  lua_rawget(L, -3);
  check(L, lua_isnil(L, -1), "luaL_getsubtable __newindex does not raw set");
  lua_pop(L, 4);

  check(L, luaL_getsubtable(L, LUA_REGISTRYINDEX, LUA_LOADED_TABLE) == 1,
	"LUA_LOADED_TABLE");
  lua_pop(L, 1);

  gs = luaL_gsub(L, "a-b-a", "-", "_");
  check(L, gs != NULL && strcmp(gs, "a_b_a") == 0,
	"luaL_gsub return value");
  check_string(L, -1, "a_b_a", "luaL_gsub pushes result");
  lua_pop(L, 1);

  check_fresh_invalid_value(L, laux_ref_missing_value,
			    "luaL_ref rejects missing value",
			    "luaL_ref missing value error");

  lua_newtable(L);
  lua_pushliteral(L, "ref-value");
  ref = luaL_ref(L, -2);
  check(L, ref > 0, "luaL_ref positive ref");
  lua_rawgeti(L, -1, ref);
  check_string(L, -1, "ref-value", "luaL_ref stored value");
  lua_pop(L, 1);
  luaL_unref(L, -1, ref);
  lua_rawgeti(L, -1, ref);
  check_integer(L, -1, 0, "luaL_unref chains ref to empty freelist");
  lua_pop(L, 1);
  lua_pushnil(L);
  check(L, luaL_ref(L, -2) == LUA_REFNIL, "luaL_ref nil sentinel");
  luaL_unref(L, -1, LUA_NOREF);
  luaL_unref(L, -1, LUA_REFNIL);
  lua_pushliteral(L, "zero-key");
  lua_rawseti(L, -2, 0);
  lua_pushliteral(L, "ref-after-zero");
  ref = luaL_ref(L, -2);
  luaL_unref(L, -1, ref);
  lua_rawgeti(L, -1, 0);
  check_string(L, -1, "zero-key", "luaL_ref freelist preserves key 0");
  lua_pop(L, 1);
  lua_pop(L, 1);

  luaL_requiref(L, "capi.mod", require_open, 1);
  check(L, require_open_count == 1, "luaL_requiref calls opener once");
  lua_getfield(L, -1, "state");
  check_string(L, -1, "ready", "luaL_requiref module result");
  lua_pop(L, 1);
  lua_getglobal(L, "capi.mod");
  lua_getfield(L, -1, "state");
  check_string(L, -1, "ready", "luaL_requiref global result");
  lua_pop(L, 3);
  luaL_requiref(L, "capi.mod", require_open, 1);
  check(L, require_open_count == 1, "luaL_requiref reuses loaded module");
  lua_pop(L, 1);

  luaL_getsubtable(L, LUA_REGISTRYINDEX, LUA_LOADED_TABLE);
  lua_pushboolean(L, 0);
  lua_setfield(L, -2, "capi.falsemod");
  lua_pop(L, 1);
  before_count = require_open_count;
  luaL_requiref(L, "capi.falsemod", require_open_false, 0);
  check(L, require_open_count == before_count + 1,
	"luaL_requiref reloads false loaded module");
  lua_getfield(L, -1, "state");
  check_string(L, -1, "false-ready", "luaL_requiref false loaded result");
  lua_pop(L, 2);

  before_count = require_open_count;
  lua_pushliteral(L, "stale-global");
  lua_setglobal(L, "capi.nilmod");
  luaL_requiref(L, "capi.nilmod", require_open_nil, 1);
  check(L, require_open_count == before_count + 1,
	"luaL_requiref calls nil-returning opener");
  check(L, lua_isnil(L, -1), "luaL_requiref nil result");
  lua_pop(L, 1);
  lua_getglobal(L, "capi.nilmod");
  check(L, lua_isnil(L, -1), "luaL_requiref nil result clears global");
  lua_pop(L, 1);
  luaL_getsubtable(L, LUA_REGISTRYINDEX, LUA_LOADED_TABLE);
  lua_getfield(L, -1, "capi.nilmod");
  check(L, lua_isnil(L, -1), "luaL_requiref nil result stays unloaded");
  lua_pop(L, 2);
  luaL_requiref(L, "capi.nilmod", require_open_nil, 0);
  check(L, require_open_count == before_count + 2,
	"luaL_requiref reloads nil-returning module");
  check(L, lua_isnil(L, -1), "luaL_requiref nil reload result");
  lua_pop(L, 1);

  check(L, luaL_fileresult(L, 1, NULL) == 1, "luaL_fileresult success arity");
  check(L, lua_toboolean(L, -1), "luaL_fileresult success value");
  lua_pop(L, 1);

  errno = ENOENT;
  check(L, luaL_fileresult(L, 0, "missing.lua") == 3,
	"luaL_fileresult failure arity");
  check(L, lua_isnil(L, -3), "luaL_fileresult failure nil");
  check(L, strstr(lua_tostring(L, -2), "missing.lua") != NULL,
	"luaL_fileresult failure filename");
  check_integer(L, -1, ENOENT, "luaL_fileresult errno");
  lua_pop(L, 3);

  errno = 0;
  check(L, luaL_fileresult(L, 0, NULL) == 3,
	"luaL_fileresult errno-zero arity");
  check(L, lua_isnil(L, -3), "luaL_fileresult errno-zero nil");
  check_string(L, -2, "(no extra info)",
	       "luaL_fileresult errno-zero message");
  check_integer(L, -1, 0, "luaL_fileresult errno-zero code");
  lua_pop(L, 3);

  check(L, luaL_execresult(L, 0) == 3, "luaL_execresult success arity");
  check(L, lua_toboolean(L, -3), "luaL_execresult success bool");
  check_string(L, -2, "exit", "luaL_execresult success kind");
  check_integer(L, -1, 0, "luaL_execresult success code");
  lua_pop(L, 3);

  errno = EACCES;
  check(L, luaL_execresult(L, 1) == 3,
	"luaL_execresult errno failure arity");
  check(L, lua_isnil(L, -3), "luaL_execresult errno failure nil");
  check(L, strstr(lua_tostring(L, -2), strerror(EACCES)) != NULL,
	"luaL_execresult errno failure message");
  check_integer(L, -1, EACCES, "luaL_execresult errno failure code");
  lua_pop(L, 3);

  luaL_newlib(L, capi_newlib);
  lua_getfield(L, -1, "answer");
  lua_call(L, 0, 1);
  check_integer(L, -1, 42, "luaL_newlib function");
  lua_pop(L, 2);
  luaL_newlibtable(L, capi_newlib);
  check(L, lua_istable(L, -1), "luaL_newlibtable table");
  check(L, lua_getfield(L, -1, "answer") == LUA_TNIL,
	"luaL_newlibtable leaves functions unset");
  lua_pop(L, 2);
  test_newlib_macro_checkversion(L);

  lua_newtable(L);
  lua_pushliteral(L, "captured-upvalue");
  luaL_setfuncs(L, capi_setfuncs, 1);
  lua_getfield(L, -1, "upvalue");
  lua_call(L, 0, 1);
  check_string(L, -1, "captured-upvalue", "luaL_setfuncs upvalue");
  lua_pop(L, 2);

  lua_newtable(L);
  lua_pushliteral(L, "placeholder-upvalue");
  luaL_setfuncs(L, capi_setfuncs_placeholder, 1);
  check(L, lua_gettop(L) >= 1 && lua_istable(L, -1),
	"luaL_setfuncs placeholder leaves table");
  lua_getfield(L, -1, "upvalue");
  lua_call(L, 0, 1);
  check_string(L, -1, "placeholder-upvalue",
	       "luaL_setfuncs placeholder keeps real closure upvalue");
  lua_pop(L, 1);
  check(L, lua_getfield(L, -1, "placeholder") == LUA_TBOOLEAN &&
	   lua_toboolean(L, -1) == 0,
	"luaL_setfuncs NULL placeholder becomes false");
  lua_pop(L, 2);
  check_fresh_invalid_value(L, setfuncs_null_list,
			    "luaL_setfuncs rejects NULL list",
			    "luaL_setfuncs NULL list error");
  check_fresh_invalid_value(L, setfuncs_negative_upvalues,
			    "luaL_setfuncs rejects negative upvalues",
			    "luaL_setfuncs negative upvalues error");
  check_fresh_invalid_value(L, setfuncs_missing_target,
			    "luaL_setfuncs rejects missing target",
			    "luaL_setfuncs missing target error");
  check_fresh_invalid_value(L, setfuncs_missing_upvalue,
			    "luaL_setfuncs rejects missing upvalue",
			    "luaL_setfuncs missing upvalue error");

  check(L, luaL_newmetatable(L, "capi.ud") == 1, "luaL_newmetatable creates");
  rtype = lua_getfield(L, -1, "__name");
  check(L, rtype == LUA_TSTRING, "luaL_newmetatable __name type");
  check_string(L, -1, "capi.ud", "luaL_newmetatable __name value");
  lua_pop(L, 2);
  check(L, luaL_getmetatable(L, "capi.ud") == LUA_TTABLE,
	"luaL_getmetatable type");
  lua_pop(L, 1);
  ud = lua_newuserdatauv(L, 1, 0);
  luaL_setmetatable(L, "capi.ud");
  check(L, luaL_testudata(L, -1, "capi.ud") == ud, "luaL_testudata match");
  check(L, luaL_testudata(L, -1, "capi.other") == NULL,
	"luaL_testudata mismatch");
  check(L, luaL_checkudata(L, -1, "capi.ud") == ud, "luaL_checkudata match");
  lua_pop(L, 1);

  lua_newtable(L);
  check(L, strcmp(luaL_typename(L, -1), "table") == 0,
	"luaL_typename table");
  lua_newtable(L);
  lua_pushliteral(L, "CapiMeta");
  lua_setfield(L, -2, "__name");
  lua_pushcfunction(L, capi_tostring_meta);
  lua_setfield(L, -2, "__tostring");
  lua_setmetatable(L, -2);
  check(L, luaL_getmetafield(L, -1, "__name") == LUA_TSTRING,
	"luaL_getmetafield returns field type");
  check_string(L, -1, "CapiMeta", "luaL_getmetafield value");
  lua_pop(L, 1);
  {
    int top = lua_gettop(L);
    check(L, luaL_getmetafield(L, -1, "__missing") == 0,
	  "luaL_getmetafield missing");
    check(L, lua_gettop(L) == top, "luaL_getmetafield missing stack");
  }
  {
    int top = lua_gettop(L);
    lua_newtable(L);
    lua_newtable(L);
    lua_newtable(L);
    lua_pushcfunction(L, capi_metafield_index);
    lua_setfield(L, -2, "__index");
    lua_setmetatable(L, -2);
    lua_setmetatable(L, -2);
    check(L, luaL_getmetafield(L, -1, "__virtual") == LUA_TNIL,
	  "luaL_getmetafield ignores metatable __index");
    lua_pop(L, 1);
    check(L, lua_gettop(L) == top, "luaL_getmetafield raw-missing stack");
  }
  check(L, luaL_callmeta(L, -1, "__tostring") == 1,
	"luaL_callmeta calls metamethod");
  check_string(L, -1, "meta tostring", "luaL_callmeta result");
  lua_pop(L, 1);
  {
    int top = lua_gettop(L);
    check(L, luaL_callmeta(L, -1, "__missing") == 0,
	  "luaL_callmeta missing");
    check(L, lua_gettop(L) == top, "luaL_callmeta missing stack");
  }
  lua_pop(L, 1);

  luaL_where(L, 0);
  check(L, lua_isstring(L, -1), "luaL_where pushes string");
  lua_pop(L, 1);
  lua_pushcfunction(L, laux_error_arg);
  status = lua_pcall(L, 0, 0, 0);
  check(L, status == LUA_ERRRUN, "luaL_error status");
  check(L, strstr(lua_tostring(L, -1), "laux boom") != NULL,
	"luaL_error message");
  lua_pop(L, 1);
  if (sizeof(lua_Integer) > sizeof(int)) {
    lua_pushcfunction(L, laux_error_wide_integer_arg);
    status = lua_pcall(L, 0, 0, 0);
    check(L, status == LUA_ERRRUN, "luaL_error wide integer status");
    check(L, strstr(lua_tostring(L, -1), "laux big 1099511627776") != NULL,
	  "luaL_error wide integer message");
    lua_pop(L, 1);
  }

  luaL_traceback(L, L, "trace-msg", 0);
  check(L, strstr(lua_tostring(L, -1), "trace-msg") != NULL,
	"luaL_traceback message");
  check(L, strstr(lua_tostring(L, -1), "stack traceback") != NULL,
	"luaL_traceback stack");
  lua_pop(L, 1);
  check_fresh_invalid_value(L, traceback_null_thread,
			    "luaL_traceback rejects NULL target thread",
			    "luaL_traceback NULL target thread error");

  status = luaL_dostring(L, "return 12");
  check(L, status == LUA_OK, "luaL_dostring status");
  check_integer(L, -1, 12, "luaL_dostring result");
  lua_pop(L, 1);

  tmpf = fopen(tmpname, "wb");
  check(L, tmpf != NULL, "luaL_dofile temp open");
  check(L, fputs("return 13\n", tmpf) >= 0, "luaL_dofile temp write");
  check(L, fclose(tmpf) == 0, "luaL_dofile temp close");
  status = luaL_dofile(L, tmpname);
  remove(tmpname);
  check(L, status == LUA_OK, "luaL_dofile status");
  check_integer(L, -1, 13, "luaL_dofile result");
  lua_pop(L, 1);

  lua_pushboolean(L, 1);
  luaL_tolstring(L, -1, NULL);
  check_string(L, -1, "true", "luaL_tolstring boolean");
  lua_pop(L, 2);

  lua_pushinteger(L, 1);
  luaL_tolstring(L, -1, NULL);
  check_string(L, -1, "1", "luaL_tolstring integer subtype");
  lua_pop(L, 2);

  lua_pushnumber(L, (lua_Number)1.0);
  luaL_tolstring(L, -1, NULL);
  check_string(L, -1, "1.0", "luaL_tolstring float integral subtype");
  lua_pop(L, 2);

  lua_pushnumber(L, (lua_Number)1.5);
  luaL_tolstring(L, -1, NULL);
  check_string(L, -1, "1.5", "luaL_tolstring float fraction subtype");
  lua_pop(L, 2);

  lua_newtable(L);
  lua_newtable(L);
  lua_pushliteral(L, "NamedTolstring");
  lua_setfield(L, -2, "__name");
  lua_setmetatable(L, -2);
  {
    char want[96];
    int wantlen = snprintf(want, sizeof(want), "NamedTolstring: %p",
			   lua_topointer(L, -1));
    check(L, wantlen > 0 && wantlen < (int)sizeof(want),
	  "luaL_tolstring string __name expected string");
    luaL_tolstring(L, -1, NULL);
    check_string(L, -1, want, "luaL_tolstring string __name");
    lua_pop(L, 2);
  }

  lua_newtable(L);
  lua_newtable(L);
  lua_pushinteger(L, 123);
  lua_setfield(L, -2, "__name");
  lua_setmetatable(L, -2);
  {
    char want[96];
    int wantlen = snprintf(want, sizeof(want), "table: %p",
			   lua_topointer(L, -1));
    check(L, wantlen > 0 && wantlen < (int)sizeof(want),
	  "luaL_tolstring non-string __name expected string");
    luaL_tolstring(L, -1, NULL);
    check_string(L, -1, want, "luaL_tolstring ignores non-string __name");
    lua_pop(L, 2);
  }

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
  luaL_addchar(&b, 'L');
  luaL_addlstring(&b, "ua", 2);
  luaL_addstring(&b, "54");
  luaL_addlstring(&b, NULL, 0);
  lua_pushliteral(L, "!");
  luaL_addvalue(&b);
  luaL_pushresult(&b);
  check_string(L, -1, "Lua54!", "luaL_addchar/addstring/addvalue");
  lua_pop(L, 1);

  luaL_buffinit(L, &b);
  p = luaL_prepbuffer(&b);
  memcpy(p, "xy", 2);
  pushresultsize_fn(&b, 2);
  check_string(L, -1, "xy", "luaL_pushresultsize");
  lua_pop(L, 1);

  luaL_buffinit(L, &b);
  {
    size_t big = LUAL_BUFFERSIZE + 32;
    p = luaL_prepbuffsize(&b, big);
    memset(p, 'z', big);
    luaL_addsize(&b, big);
    check(L, luaL_bufflen(&b) == big, "luaL_prepbuffsize big length");
    luaL_pushresult(&b);
    check(L, lua_rawlen(L, -1) == big, "luaL_prepbuffsize big result size");
    check(L, lua_tostring(L, -1)[0] == 'z' &&
	     lua_tostring(L, -1)[big - 1] == 'z',
	  "luaL_prepbuffsize big result bytes");
    lua_pop(L, 1);
  }

  {
    size_t big = LUAL_BUFFERSIZE + 17;
    p = buffinitsize_fn(L, &b, big);
    memset(p, 'q', big);
    pushresultsize_fn(&b, big);
    check(L, lua_rawlen(L, -1) == big, "luaL_buffinitsize big result size");
    check(L, lua_tostring(L, -1)[0] == 'q' &&
	     lua_tostring(L, -1)[big - 1] == 'q',
	  "luaL_buffinitsize big result bytes");
    lua_pop(L, 1);
  }

  test_lauxlib_buffer_too_large(L);

  luaL_buffinit(L, &b);
  luaL_addgsub(&b, "a?$?", "?", "Lua54");
  luaL_pushresult(&b);
  check_string(L, -1, "aLua54$Lua54", "luaL_addgsub result");
  lua_pop(L, 1);

  check_fresh_invalid_value(L, addlstring_null_nonzero,
			    "luaL_addlstring rejects NULL nonzero string",
			    "luaL_addlstring NULL nonzero string error");
  check_fresh_invalid_value(L, addstring_null_string,
			    "luaL_addstring rejects NULL string",
			    "luaL_addstring NULL string error");
  check_fresh_invalid_value(L, buffer_prep_missing_placeholder,
			    "luaL_prepbuffsize rejects missing placeholder",
			    "luaL_prepbuffsize missing placeholder error");
  check_fresh_invalid_value(L, buffer_prep_wrong_placeholder,
			    "luaL_prepbuffsize rejects wrong placeholder",
			    "luaL_prepbuffsize wrong placeholder error");
  check_fresh_invalid_value(L, buffer_pushresult_missing_placeholder,
			    "luaL_pushresult rejects missing placeholder",
			    "luaL_pushresult missing placeholder error");
  check_fresh_invalid_value(L, buffer_addvalue_missing_value,
			    "luaL_addvalue rejects missing value",
			    "luaL_addvalue missing value error");
  check_fresh_invalid_value(L, buffer_addvalue_bad_value,
			    "luaL_addvalue rejects non-string value",
			    "luaL_addvalue non-string value error");
  check_fresh_invalid_value(L, addgsub_null_subject,
			    "luaL_addgsub rejects NULL subject",
			    "luaL_addgsub NULL subject error");
  check_fresh_invalid_value(L, addgsub_null_pattern,
			    "luaL_addgsub rejects NULL pattern",
			    "luaL_addgsub NULL pattern error");
  check_fresh_invalid_value(L, addgsub_null_replacement,
			    "luaL_addgsub rejects NULL replacement",
			    "luaL_addgsub NULL replacement error");

  status = luaL_loadbufferx(L, "return 54", 9, "=capi-buffer", "t");
  check(L, status == LUA_OK, "luaL_loadbufferx text mode");
  lua_call(L, 0, 1);
  check_integer(L, -1, 54, "luaL_loadbufferx loaded function");
  lua_pop(L, 1);

  status = luaL_loadbuffer(L, "return 55", 9, "=capi-loadbuffer");
  check(L, status == LUA_OK, "luaL_loadbuffer macro");
  lua_call(L, 0, 1);
  check_integer(L, -1, 55, "luaL_loadbuffer macro result");
  lua_pop(L, 1);

  status = luaL_loadbufferx(L, NULL, 0, "=empty-null-buffer", "t");
  check(L, status == LUA_OK, "luaL_loadbufferx accepts NULL zero length");
  lua_pop(L, 1);
  check_fresh_invalid_value(L, loadbuffer_null_nonzero,
			    "luaL_loadbufferx rejects NULL nonzero buffer",
			    "luaL_loadbufferx NULL nonzero buffer error");
  check_fresh_invalid_value(L, loadstring_null_string,
			    "luaL_loadstring rejects NULL string",
			    "luaL_loadstring NULL string error");

  status = luaL_loadbufferx(L, "return 54", 9, "=capi-buffer", "b");
  check(L, status == LUA_ERRSYNTAX, "luaL_loadbufferx binary mode rejects text");
  check(L, strstr(lua_tostring(L, -1),
		  "attempt to load a text chunk (mode is 'b')") != NULL,
	"luaL_loadbufferx wrong mode error");
  lua_pop(L, 1);

  status = luaL_loadfile(L, "test/smoke.lua");
  check(L, status == LUA_OK, "luaL_loadfile macro");
  lua_pop(L, 1);

  status = luaL_loadfilex(L, "test/smoke.lua", "t");
  check(L, status == LUA_OK, "luaL_loadfilex text mode");
  lua_pop(L, 1);

  status = luaL_loadfilex(L, "test/smoke.lua", "b");
  check(L, status == LUA_ERRSYNTAX, "luaL_loadfilex binary mode rejects text");
  check(L, strstr(lua_tostring(L, -1),
		  "attempt to load a text chunk (mode is 'b')") != NULL,
	"luaL_loadfilex wrong mode error");
  lua_pop(L, 1);

  status = luaL_loadfilex(L, "test/does_not_exist_lua54_capi.lua", "t");
  check(L, status == LUA_ERRFILE, "luaL_loadfilex missing file status");
  check(L, strstr(lua_tostring(L, -1),
		  "cannot open test/does_not_exist_lua54_capi.lua:") != NULL,
	"luaL_loadfilex missing file error");
  lua_pop(L, 1);

  memset(&dump, 0, sizeof(dump));
  status = luaL_loadbufferx(L, "return 79", 9, "=capi-dump-file", "t");
  check(L, status == LUA_OK, "luaL_loadfilex binary setup load");
  check(L, lua_dump_sig(L, dump_writer, &dump, 0) == 0,
	"luaL_loadfilex binary setup dump");
  lua_pop(L, 1);
  tmpf = fopen(binname, "wb");
  check(L, tmpf != NULL, "luaL_loadfilex hash binary temp open");
  check(L, fputs("# lua54 capi binary\n", tmpf) >= 0,
	"luaL_loadfilex hash binary header write");
  check(L, fwrite(dump.data, 1, dump.len, tmpf) == dump.len,
	"luaL_loadfilex hash binary body write");
  check(L, fclose(tmpf) == 0, "luaL_loadfilex hash binary close");
  status = luaL_loadfilex(L, binname, "b");
  check(L, status == LUA_OK, "luaL_loadfilex hash binary mode");
  lua_call(L, 0, 1);
  check_integer(L, -1, 79, "luaL_loadfilex hash binary result");
  lua_pop(L, 1);
  status = luaL_loadfilex(L, binname, "t");
  remove(binname);
  check(L, status == LUA_ERRSYNTAX,
	"luaL_loadfilex text mode rejects hash binary");
  check(L, strstr(lua_tostring(L, -1),
		  "attempt to load a binary chunk (mode is 't')") != NULL,
	"luaL_loadfilex hash binary wrong mode error");
  lua_pop(L, 1);

  reader.src = "return 64";
  reader.len = 9;
  status = lua_load_sig(L, capi_reader, &reader, "=capi-reader", "t");
  check(L, status == LUA_OK, "lua_load function pointer text mode");
  lua_call(L, 0, 1);
  check_integer(L, -1, 64, "lua_load loaded function");
  lua_pop(L, 1);

  reader.src = "return 64";
  reader.len = 9;
  status = lua_load_sig(L, capi_reader, &reader, "=capi-reader", "b");
  check(L, status == LUA_ERRSYNTAX,
	"lua_load function pointer binary mode rejects text");
  check(L, strstr(lua_tostring(L, -1),
		  "attempt to load a text chunk (mode is 'b')") != NULL,
	"lua_load wrong mode error");
  lua_pop(L, 1);

  check_fresh_invalid_value(L, load_null_reader,
			    "lua_load rejects NULL reader",
			    "lua_load NULL reader error");
}

static void test_dump_api(lua_State *L)
{
  const char *src = "return function(a) return a + 1 end";
  DumpBuffer full;
  DumpBuffer stripped;
  CApiReaderCtx reader;
  int fail_calls = 0;
  int status;

  memset(&full, 0, sizeof(full));
  memset(&stripped, 0, sizeof(stripped));

  status = luaL_loadbufferx(L, src, strlen(src), "=dump-source", "t");
  check(L, status == LUA_OK, "lua_dump setup load");
  lua_call(L, 0, 1);
  check(L, lua_dump_sig(L, dump_writer, &full, 0) == 0,
	"lua_dump function pointer full");
  check(L, full.len > 0, "lua_dump full length");
  check(L, lua_dump_sig(L, dump_writer, &stripped, 1) == 0,
	"lua_dump function pointer stripped");
  check(L, stripped.len > 0 && stripped.len <= full.len,
	"lua_dump stripped length");
  check(L, lua_dump_sig(L, dump_fail_writer, &fail_calls, 0) == 77,
	"lua_dump writer failure status");
  check(L, fail_calls > 0, "lua_dump writer failure callback");
  lua_pop(L, 1);
  fail_calls = 0;
  lua_pushcfunction(L, checkinteger_arg);
  check(L, lua_dump_sig(L, dump_fail_writer, &fail_calls, 0) == 1,
	"lua_dump C function status");
  check(L, fail_calls == 0, "lua_dump C function skips writer");
  lua_pop(L, 1);

  check_fresh_invalid_value(L, dump_empty_stack,
			    "lua_dump rejects empty stack",
			    "lua_dump empty stack error");
  check_fresh_invalid_value(L, dump_null_writer,
			    "lua_dump rejects NULL writer",
			    "lua_dump NULL writer error");

  status = luaL_loadbufferx(L, stripped.data, stripped.len, "=dumped", "b");
  check(L, status == LUA_OK, "lua_dump stripped reload");
  lua_pushinteger(L, 41);
  lua_call(L, 1, 1);
  check_integer(L, -1, 42, "lua_dump stripped roundtrip");
  lua_pop(L, 1);

  status = luaL_loadbufferx(L, stripped.data, stripped.len, "=dumped", "t");
  check(L, status == LUA_ERRSYNTAX,
	"luaL_loadbufferx text mode rejects binary");
  check(L, strstr(lua_tostring(L, -1),
		  "attempt to load a binary chunk (mode is 't')") != NULL,
	"luaL_loadbufferx binary wrong mode error");
  lua_pop(L, 1);

  reader.src = stripped.data;
  reader.len = stripped.len;
  status = lua_load_sig(L, capi_reader, &reader, "=dumped-reader", "b");
  check(L, status == LUA_OK, "lua_load binary reader mode");
  lua_pushinteger(L, 41);
  lua_call(L, 1, 1);
  check_integer(L, -1, 42, "lua_load binary reader result");
  lua_pop(L, 1);

  reader.src = stripped.data;
  reader.len = stripped.len;
  status = lua_load_sig(L, capi_reader, &reader, "=dumped-reader", "t");
  check(L, status == LUA_ERRSYNTAX,
	"lua_load text mode rejects binary reader");
  check(L, strstr(lua_tostring(L, -1),
		  "attempt to load a binary chunk (mode is 't')") != NULL,
	"lua_load binary reader wrong mode error");
  lua_pop(L, 1);
}

static void test_warning_and_gc_api(lua_State *L)
{
  int oldmode;
  int oldpause;
  int oldmul;
  int stepdone;
  int countb;
  int status;
  lua_Number version;
  lua_Debug ar;

  version = lua_version(L);
  check(L, version == (lua_Number)LUA_VERSION_NUM,
	"lua_version returns numeric Lua 5.4 version");
  version = lua_version(NULL);
  check(L, version == (lua_Number)LUA_VERSION_NUM,
	"lua_version accepts NULL state");
  version = lua_version_value_sig(L);
  check(L, version == (lua_Number)LUA_VERSION_NUM,
	"lua_version function pointer returns Lua 5.4 version");
  version = lua_version_value_sig(NULL);
  check(L, version == (lua_Number)LUA_VERSION_NUM,
	"lua_version function pointer accepts NULL state");
  check(L, lua_setcstacklimit(L, 0) == 200,
	"lua_setcstacklimit query shim");
  check(L, lua_setcstacklimit(L, 200) == 200,
	"lua_setcstacklimit set shim");

  lua_setwarnf(L, capture_warning, NULL);
  lua_warning(L, "captured", 0);
  check(L, strcmp(warning_buf, "captured") == 0 && warning_tocont == 0,
	"lua_warning callback");
  check_fresh_invalid_value(L, warning_null_message,
			    "lua_warning rejects NULL message",
			    "lua_warning NULL message error");

  check(L, lua_gc(L, LUA_GCCOLLECT) == 0, "lua_gc vararg collect");
  (void)lua_gc(L, LUA_GCCOUNT);
  countb = lua_gc(L, LUA_GCCOUNTB);
  check(L, countb >= 0 && countb < 1024, "LUA_GCCOUNTB byte remainder");
  check(L, lua_gc(L, LUA_GCISRUNNING) == 1, "lua_gc initial isrunning");
  check(L, lua_gc(L, LUA_GCSTOP) == 0, "lua_gc stop");
  check(L, lua_gc(L, LUA_GCISRUNNING) == 0, "lua_gc stopped isrunning");
  check(L, lua_gc(L, LUA_GCCOLLECT) == 0, "lua_gc collect while stopped");
  check(L, lua_gc(L, LUA_GCISRUNNING) == 0,
	"lua_gc collect must not restart stopped collector");
  stepdone = lua_gc(L, LUA_GCSTEP, 20000);
  check(L, stepdone == 0 || stepdone == 1, "lua_gc step boolean result");
  check(L, lua_gc(L, LUA_GCISRUNNING) == 0,
	"lua_gc step must not restart stopped collector");
  check(L, lua_gc(L, LUA_GCRESTART) == 0, "lua_gc restart");
  check(L, lua_gc(L, LUA_GCISRUNNING) == 1, "lua_gc restarted isrunning");
  oldmode = lua_gc(L, LUA_GCINC, 200, 100, 13);
  check(L, oldmode == LUA_GCGEN || oldmode == LUA_GCINC,
	"LUA_GCINC before step return mode");
  check(L, lua_gc(L, LUA_GCCOLLECT) == 0,
	"lua_gc collect before incremental step");
  check(L, lua_gc(L, LUA_GCSTEP, -1) == 0,
	"LUA_GCSTEP negative reports incomplete");
  check(L, lua_gc(L, LUA_GCSTEP, 0) == 1,
	"LUA_GCSTEP zero reports completed pause cycle");
  oldpause = lua_gc(L, LUA_GCSETPAUSE, 123);
  check(L, oldpause == 200, "lua_gc setpause initial value");
  check(L, lua_gc(L, LUA_GCSETPAUSE, oldpause) == 120,
	"lua_gc setpause Lua 5.4 quantized old value");
  oldmul = lua_gc(L, LUA_GCSETSTEPMUL, 321);
  check(L, oldmul == 100, "lua_gc setstepmul initial value");
  check(L, lua_gc(L, LUA_GCSETSTEPMUL, oldmul) == 320,
	"lua_gc setstepmul Lua 5.4 quantized old value");

  oldmode = lua_gc(L, LUA_GCGEN, 0, 0);
  check(L, oldmode == LUA_GCGEN || oldmode == LUA_GCINC, "LUA_GCGEN");
  oldmode = lua_gc(L, LUA_GCINC, 0, 0, 0);
  check(L, oldmode == LUA_GCGEN, "LUA_GCINC previous mode");
  oldmode = lua_gc(L, LUA_GCGEN, 0, 0);
  check(L, oldmode == LUA_GCINC, "LUA_GCGEN previous mode");
  oldmode = lua_gc(L, LUA_GCINC, 321, 432, 13);
  check(L, oldmode == LUA_GCGEN, "LUA_GCINC updates previous mode");
  check(L, lua_gc(L, LUA_GCSETPAUSE, 200) == 320,
	"LUA_GCINC updates pause");
  check(L, lua_gc(L, LUA_GCSETSTEPMUL, 100) == 432,
	"LUA_GCINC updates stepmul");
  oldmode = lua_gc(L, LUA_GCGEN, 21, 155);
  check(L, oldmode == LUA_GCINC, "LUA_GCGEN stores parameters");
  check(L, lua_gc(L, LUA_GCSTEP, 0) == 0,
	"LUA_GCSTEP generational reports no incremental cycle");
  check(L, lua_gc(L, LUA_GCSTOP) == 0, "lua_gc stop before mode switch");
  oldmode = lua_gc(L, LUA_GCINC, 321, 432, 13);
  check(L, oldmode == LUA_GCGEN, "LUA_GCINC stopped previous mode");
  check(L, lua_gc(L, LUA_GCISRUNNING) == 0,
	"LUA_GCINC must not restart stopped collector");
  oldmode = lua_gc(L, LUA_GCGEN, 21, 155);
  check(L, oldmode == LUA_GCINC, "LUA_GCGEN stopped previous mode");
  check(L, lua_gc(L, LUA_GCISRUNNING) == 0,
	"LUA_GCGEN must not restart stopped collector");
  check(L, lua_gc(L, LUA_GCRESTART) == 0,
	"lua_gc restart after stopped mode switch");

  lua_pushcfunction(L, capi_gc_reentry);
  lua_setglobal(L, "capi_gc_reentry");
  status = luaL_dostring(L,
    "local a, b\n"
    "do\n"
    "  local x = setmetatable({}, { __gc = function()\n"
    "    a, b = capi_gc_reentry()\n"
    "  end })\n"
    "  x = nil\n"
    "end\n"
    "collectgarbage('collect')\n"
    "assert(a == -1 and b == -1)\n");
  check(L, status == LUA_OK, "lua_gc rejects C API reentry in finalizer");
  lua_pushnil(L);
  lua_setglobal(L, "capi_gc_reentry");

  check(L, lua_getlocal(L, NULL, 1) == NULL,
	"lua_getlocal NULL debug without function returns NULL");
  check_fresh_invalid_value(L, getinfo_null_what,
			    "lua_getinfo rejects NULL what",
			    "lua_getinfo NULL what error");
  check_fresh_invalid_value(L, getinfo_null_debug,
			    "lua_getinfo rejects NULL debug record",
			    "lua_getinfo NULL debug record error");
  check_fresh_invalid_value(L, getstack_null_debug,
			    "lua_getstack rejects NULL debug record",
			    "lua_getstack NULL debug record error");
  check_fresh_invalid_value(L, setlocal_null_debug,
			    "lua_setlocal rejects NULL debug record",
			    "lua_setlocal NULL debug record error");

  lua_pushcfunction(L, setlocal_missing_name_preserves_stack);
  status = lua_pcall(L, 0, 0, 0);
  check(L, status == LUA_OK, "lua_setlocal missing name preserves stack");

  memset(&ar, 0, sizeof(ar));
  lua_pushcfunction(L, push_answer);
  check(L, lua_getinfo(L, ">Sutr", &ar), "lua_getinfo >Sutr");
  check(L, ar.source && ar.srclen == strlen(ar.source),
	"lua_Debug srclen matches C source length");
  check(L, ar.nparams == 0 && ar.isvararg == 1, "lua_Debug u fields");
  check(L, ar.istailcall == 0 && ar.ftransfer == 0 && ar.ntransfer == 0,
	"lua_Debug t/transfer fields");

  memset(&ar, 0, sizeof(ar));
  check(L, luaL_loadbuffer(L, "return 1", 8, "lua54_srclen_probe") == LUA_OK,
	"load lua_Debug srclen probe");
  check(L, lua_getinfo(L, ">S", &ar), "lua_getinfo >S srclen");
  check(L, ar.source && ar.srclen == strlen("lua54_srclen_probe") &&
	strcmp(ar.source, "lua54_srclen_probe") == 0,
	"lua_Debug srclen matches Lua chunk source length");

  lua_sethook(L, capi_transfer_hook, LUA_MASKCOUNT, 7);
  check(L, lua_gethook(L) == capi_transfer_hook, "lua_gethook set hook");
  check(L, lua_gethookmask(L) == LUA_MASKCOUNT, "lua_gethookmask set mask");
  check(L, lua_gethookcount(L) == 7, "lua_gethookcount set count");
  lua_sethook(L, NULL, 0, 0);
  check(L, lua_gethook(L) == NULL, "lua_gethook clear hook");
  check(L, lua_gethookmask(L) == 0, "lua_gethookmask clear mask");
  check(L, lua_gethookcount(L) == 0, "lua_gethookcount clear count");

  hook_call_ftransfer = hook_call_ntransfer = -1;
  hook_ret_ftransfer = hook_ret_ntransfer = -1;
  lua_sethook(L, capi_transfer_hook, LUA_MASKCALL | LUA_MASKRET, 0);
  status = luaL_dostring(L,
    "local function capi_transfer_probe(a, b) return a + b, a - b end\n"
    "local x, y = capi_transfer_probe(3, 1); return x, y");
  lua_sethook(L, NULL, 0, 0);
  check(L, status == LUA_OK, "lua_getinfo hook transfer setup");
  check_integer(L, -2, 4, "lua_getinfo hook transfer result 1");
  check_integer(L, -1, 2, "lua_getinfo hook transfer result 2");
  lua_pop(L, 2);
  check(L, hook_call_ftransfer == 1 && hook_call_ntransfer == 2,
	"lua_getinfo call hook transfer fields");
  check(L, hook_ret_ftransfer == 3 && hook_ret_ntransfer == 2,
	"lua_getinfo return hook transfer fields");

  hook_vararg_call_ftransfer = hook_vararg_call_ntransfer = -1;
  hook_vararg_ret_ftransfer = hook_vararg_ret_ntransfer = -1;
  lua_sethook(L, capi_transfer_hook, LUA_MASKCALL | LUA_MASKRET, 0);
  status = luaL_dostring(L,
    "local function capi_transfer_vararg(a, ...) return a, ... end\n"
    "local a, b, c = capi_transfer_vararg(1, 2, 3); return a, b, c");
  lua_sethook(L, NULL, 0, 0);
  check(L, status == LUA_OK, "lua_getinfo vararg hook transfer setup");
  check_integer(L, -3, 1, "lua_getinfo vararg transfer result 1");
  check_integer(L, -2, 2, "lua_getinfo vararg transfer result 2");
  check_integer(L, -1, 3, "lua_getinfo vararg transfer result 3");
  lua_pop(L, 3);
  check(L, hook_vararg_call_ftransfer == 1 &&
	hook_vararg_call_ntransfer == 1,
	"lua_getinfo vararg call hook transfer fields");
  check(L, hook_vararg_ret_ftransfer == 2 &&
	hook_vararg_ret_ntransfer == 3,
	"lua_getinfo vararg return hook transfer fields");

  hook_tail_ftransfer = hook_tail_ntransfer = -1;
  hook_tail_istailcall = -1;
  lua_sethook(L, capi_transfer_hook, LUA_MASKCALL | LUA_MASKRET, 0);
  status = luaL_dostring(L,
    "local function capi_tail_target(a, b) return a + b end\n"
    "local function capi_tail_caller(a, b) return capi_tail_target(a, b) end\n"
    "local x = capi_tail_caller(2, 3); return x");
  lua_sethook(L, NULL, 0, 0);
  check(L, status == LUA_OK, "lua_getinfo tail hook transfer setup");
  check_integer(L, -1, 5, "lua_getinfo tail hook transfer result");
  lua_pop(L, 1);
  check(L, hook_tail_istailcall == 1 &&
	hook_tail_ftransfer == 1 && hook_tail_ntransfer == 2,
	"lua_getinfo tail call hook transfer fields");

  hook_tail_vararg_call_ftransfer = hook_tail_vararg_call_ntransfer = -1;
  hook_tail_vararg_ret_ftransfer = hook_tail_vararg_ret_ntransfer = -1;
  hook_tail_vararg_ret_istailcall = -1;
  lua_sethook(L, capi_transfer_hook, LUA_MASKCALL | LUA_MASKRET, 0);
  status = luaL_dostring(L,
    "local function capi_tail_vararg_target(...) return ... end\n"
    "local function capi_tail_vararg_caller(...) "
      "return capi_tail_vararg_target(...) end\n"
    "local a, b, c = capi_tail_vararg_caller(1, 2, 3); return a, b, c");
  lua_sethook(L, NULL, 0, 0);
  check(L, status == LUA_OK, "lua_getinfo vararg tail hook setup");
  check_integer(L, -3, 1, "lua_getinfo vararg tail result 1");
  check_integer(L, -2, 2, "lua_getinfo vararg tail result 2");
  check_integer(L, -1, 3, "lua_getinfo vararg tail result 3");
  lua_pop(L, 3);
  check(L, hook_tail_vararg_call_ftransfer == 0 &&
	hook_tail_vararg_call_ntransfer == 0,
	"lua_getinfo vararg tail call hook transfer fields");
  check(L, hook_tail_vararg_ret_istailcall == 1 &&
	hook_tail_vararg_ret_ftransfer == 1 &&
	hook_tail_vararg_ret_ntransfer == 3,
	"lua_getinfo vararg tail return hook transfer fields");

  lua_pushcfunction(L, capi_transfer_cfunc);
  lua_setglobal(L, "capi_transfer_cfunc");
  hook_c_call_ftransfer = hook_c_call_ntransfer = -1;
  hook_c_ret_ftransfer = hook_c_ret_ntransfer = -1;
  lua_sethook(L, capi_transfer_hook, LUA_MASKCALL | LUA_MASKRET, 0);
  status = luaL_dostring(L,
    "local x = capi_transfer_cfunc(1, 2, 3); return x");
  lua_sethook(L, NULL, 0, 0);
  check(L, status == LUA_OK, "lua_getinfo C hook transfer setup");
  check_integer(L, -1, 6, "lua_getinfo C hook transfer result");
  lua_pop(L, 1);
  check(L, hook_c_call_ftransfer == 1 && hook_c_call_ntransfer == 3,
	"lua_getinfo C call hook transfer fields");
  check(L, hook_c_ret_ftransfer == 4 && hook_c_ret_ntransfer == 1,
	"lua_getinfo C return hook transfer fields");

  hook_c_call_ftransfer = hook_c_call_ntransfer = -1;
  hook_c_ret_ftransfer = hook_c_ret_ntransfer = -1;
  hook_tail_istailcall = -1;
  lua_sethook(L, capi_transfer_hook, LUA_MASKCALL | LUA_MASKRET, 0);
  status = luaL_dostring(L,
    "local function capi_tail_c_position(a, b, c) "
      "return capi_transfer_cfunc(a, b, c) end\n"
    "local x = capi_tail_c_position(1, 2, 3); return x");
  lua_sethook(L, NULL, 0, 0);
  check(L, status == LUA_OK, "lua_getinfo tail-position C hook setup");
  check_integer(L, -1, 6, "lua_getinfo tail-position C result");
  lua_pop(L, 1);
  check(L, hook_tail_istailcall < 0,
	"lua_getinfo tail-position C must not report Lua tail hook");
  check(L, hook_c_call_ftransfer == 1 && hook_c_call_ntransfer == 3,
	"lua_getinfo tail-position C call transfer fields");
  check(L, hook_c_ret_ftransfer == 4 && hook_c_ret_ntransfer == 1,
	"lua_getinfo tail-position C return transfer fields");
}

int main(void)
{
  lua_State *L = luaL_newstate();
  if (L == NULL)
    return 2;
  luaL_openlibs(L);
  test_state_allocator_api(L);
  test_stack_and_number_api(L);
  test_compare_len_arith(L);
  test_uservalue_api(L);
  test_metatable_api54(L);
  test_upvalue_api54(L);
  test_lauxlib_api(L);
  test_dump_api(L);
  test_warning_and_gc_api(L);
  lua_close(L);
  return 0;
}
