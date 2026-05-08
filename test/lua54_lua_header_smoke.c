/*
** Header-surface smoke test for lua.h alone in Lua 5.4 compatibility mode.
*/

#include "lua.h"

#ifdef LUAJIT_EXTERNAL_LUA54
#error "LUAJIT_EXTERNAL_LUA54 is a private header selector and must not leak"
#endif

#if defined(__GNUC__) || defined(__clang__)
typedef void (*lua54_sethook_type)(lua_State *, lua_Hook, int, int);
typedef char lua54_sethook_must_return_void[
  __builtin_types_compatible_p(__typeof__(&lua_sethook), lua54_sethook_type) ? 1 : -1
];
typedef char lua54_pushglobaltable_must_return_void[
  __builtin_types_compatible_p(
    __typeof__(lua_pushglobaltable((lua_State *)0)), void) ? 1 : -1
];

#define LUA54_CHECK_SIG(name, sig) \
  typedef char lua54_sig_##name[ \
    __builtin_types_compatible_p(__typeof__(&(name)), sig) ? 1 : -1 \
  ]

#define LUA54_STR1(x) #x
#define LUA54_STR(x) LUA54_STR1(x)

typedef lua_State *(*lua54_newstate_sig)(lua_Alloc, void *);
typedef void (*lua54_close_sig)(lua_State *);
typedef lua_State *(*lua54_newthread_sig)(lua_State *);
typedef int (*lua54_closethread_sig)(lua_State *, lua_State *);
typedef int (*lua54_resetthread_sig)(lua_State *);
typedef lua_CFunction (*lua54_atpanic_sig)(lua_State *, lua_CFunction);
typedef lua_Number (*lua54_version_sig)(lua_State *);
typedef int (*lua54_absindex_sig)(lua_State *, int);
typedef int (*lua54_gettop_sig)(lua_State *);
typedef void (*lua54_settop_sig)(lua_State *, int);
typedef void (*lua54_pushvalue_sig)(lua_State *, int);
typedef void (*lua54_rotate_sig)(lua_State *, int, int);
typedef void (*lua54_copy_sig)(lua_State *, int, int);
typedef int (*lua54_checkstack_sig)(lua_State *, int);
typedef void (*lua54_xmove_sig)(lua_State *, lua_State *, int);
typedef int (*lua54_int_idx_sig)(lua_State *, int);
typedef const char *(*lua54_typename_sig)(lua_State *, int);
typedef lua_Number (*lua54_tonumberx_sig)(lua_State *, int, int *);
typedef lua_Integer (*lua54_tointegerx_sig)(lua_State *, int, int *);
typedef const char *(*lua54_tolstring_sig)(lua_State *, int, size_t *);
typedef lua_Unsigned (*lua54_rawlen_sig)(lua_State *, int);
typedef lua_CFunction (*lua54_tocfunction_sig)(lua_State *, int);
typedef void *(*lua54_touserdata_sig)(lua_State *, int);
typedef lua_State *(*lua54_tothread_sig)(lua_State *, int);
typedef const void *(*lua54_topointer_sig)(lua_State *, int);
typedef void (*lua54_arith_sig)(lua_State *, int);
typedef int (*lua54_rawequal_sig)(lua_State *, int, int);
typedef int (*lua54_compare_sig)(lua_State *, int, int, int);
typedef void (*lua54_pushnil_sig)(lua_State *);
typedef void (*lua54_pushnumber_sig)(lua_State *, lua_Number);
typedef void (*lua54_pushinteger_sig)(lua_State *, lua_Integer);
typedef const char *(*lua54_pushlstring_sig)(lua_State *, const char *, size_t);
typedef const char *(*lua54_pushstring_sig)(lua_State *, const char *);
typedef const char *(*lua54_pushvfstring_sig)(lua_State *, const char *, va_list);
typedef const char *(*lua54_pushfstring_sig)(lua_State *, const char *, ...);
typedef void (*lua54_pushcclosure_sig)(lua_State *, lua_CFunction, int);
typedef void (*lua54_pushboolean_sig)(lua_State *, int);
typedef void (*lua54_pushlightuserdata_sig)(lua_State *, void *);
typedef int (*lua54_pushthread_sig)(lua_State *);
typedef int (*lua54_getglobal_sig)(lua_State *, const char *);
typedef int (*lua54_gettable_sig)(lua_State *, int);
typedef int (*lua54_getfield_sig)(lua_State *, int, const char *);
typedef int (*lua54_geti_sig)(lua_State *, int, lua_Integer);
typedef int (*lua54_rawgetp_sig)(lua_State *, int, const void *);
typedef void (*lua54_createtable_sig)(lua_State *, int, int);
typedef void *(*lua54_newuserdatauv_sig)(lua_State *, size_t, int);
typedef int (*lua54_getiuservalue_sig)(lua_State *, int, int);
typedef void (*lua54_setglobal_sig)(lua_State *, const char *);
typedef void (*lua54_setfield_sig)(lua_State *, int, const char *);
typedef void (*lua54_seti_sig)(lua_State *, int, lua_Integer);
typedef void (*lua54_rawseti_sig)(lua_State *, int, lua_Integer);
typedef void (*lua54_rawsetp_sig)(lua_State *, int, const void *);
typedef int (*lua54_setiuservalue_sig)(lua_State *, int, int);
typedef void (*lua54_callk_sig)(lua_State *, int, int, lua_KContext, lua_KFunction);
typedef int (*lua54_pcallk_sig)(lua_State *, int, int, int, lua_KContext, lua_KFunction);
typedef int (*lua54_load_sig)(lua_State *, lua_Reader, void *, const char *, const char *);
typedef int (*lua54_dump_sig)(lua_State *, lua_Writer, void *, int);
typedef int (*lua54_yieldk_sig)(lua_State *, int, lua_KContext, lua_KFunction);
typedef int (*lua54_resume_sig)(lua_State *, lua_State *, int, int *);
typedef int (*lua54_status_sig)(lua_State *);
typedef int (*lua54_isyieldable_sig)(lua_State *);
typedef void (*lua54_setwarnf_sig)(lua_State *, lua_WarnFunction, void *);
typedef void (*lua54_warning_sig)(lua_State *, const char *, int);
typedef int (*lua54_gc_sig)(lua_State *, int, ...);
typedef int (*lua54_error_sig)(lua_State *);
typedef int (*lua54_next_sig)(lua_State *, int);
typedef void (*lua54_concat_sig)(lua_State *, int);
typedef void (*lua54_len_sig)(lua_State *, int);
typedef size_t (*lua54_stringtonumber_sig)(lua_State *, const char *);
typedef lua_Alloc (*lua54_getallocf_sig)(lua_State *, void **);
typedef void (*lua54_setallocf_sig)(lua_State *, lua_Alloc, void *);
typedef void (*lua54_toclose_sig)(lua_State *, int);
typedef void (*lua54_closeslot_sig)(lua_State *, int);
typedef int (*lua54_getstack_sig)(lua_State *, int, lua_Debug *);
typedef int (*lua54_getinfo_sig)(lua_State *, const char *, lua_Debug *);
typedef const char *(*lua54_getlocal_sig)(lua_State *, const lua_Debug *, int);
typedef const char *(*lua54_getupvalue_sig)(lua_State *, int, int);
typedef void *(*lua54_upvalueid_sig)(lua_State *, int, int);
typedef void (*lua54_upvaluejoin_sig)(lua_State *, int, int, int, int);
typedef lua_Hook (*lua54_gethook_sig)(lua_State *);
typedef int (*lua54_gethookint_sig)(lua_State *);
typedef int (*lua54_setcstacklimit_sig)(lua_State *, unsigned int);

LUA54_CHECK_SIG(lua_newstate, lua54_newstate_sig);
LUA54_CHECK_SIG(lua_close, lua54_close_sig);
LUA54_CHECK_SIG(lua_newthread, lua54_newthread_sig);
LUA54_CHECK_SIG(lua_closethread, lua54_closethread_sig);
LUA54_CHECK_SIG(lua_resetthread, lua54_resetthread_sig);
LUA54_CHECK_SIG(lua_atpanic, lua54_atpanic_sig);
LUA54_CHECK_SIG(lua_version, lua54_version_sig);
LUA54_CHECK_SIG(lua_absindex, lua54_absindex_sig);
LUA54_CHECK_SIG(lua_gettop, lua54_gettop_sig);
LUA54_CHECK_SIG(lua_settop, lua54_settop_sig);
LUA54_CHECK_SIG(lua_pushvalue, lua54_pushvalue_sig);
LUA54_CHECK_SIG(lua_rotate, lua54_rotate_sig);
LUA54_CHECK_SIG(lua_copy, lua54_copy_sig);
LUA54_CHECK_SIG(lua_checkstack, lua54_checkstack_sig);
LUA54_CHECK_SIG(lua_xmove, lua54_xmove_sig);
LUA54_CHECK_SIG(lua_isnumber, lua54_int_idx_sig);
LUA54_CHECK_SIG(lua_isstring, lua54_int_idx_sig);
LUA54_CHECK_SIG(lua_iscfunction, lua54_int_idx_sig);
LUA54_CHECK_SIG(lua_isinteger, lua54_int_idx_sig);
LUA54_CHECK_SIG(lua_isuserdata, lua54_int_idx_sig);
LUA54_CHECK_SIG(lua_type, lua54_int_idx_sig);
LUA54_CHECK_SIG(lua_typename, lua54_typename_sig);
LUA54_CHECK_SIG(lua_tonumberx, lua54_tonumberx_sig);
LUA54_CHECK_SIG(lua_tointegerx, lua54_tointegerx_sig);
LUA54_CHECK_SIG(lua_toboolean, lua54_int_idx_sig);
LUA54_CHECK_SIG(lua_tolstring, lua54_tolstring_sig);
LUA54_CHECK_SIG(lua_rawlen, lua54_rawlen_sig);
LUA54_CHECK_SIG(lua_tocfunction, lua54_tocfunction_sig);
LUA54_CHECK_SIG(lua_touserdata, lua54_touserdata_sig);
LUA54_CHECK_SIG(lua_tothread, lua54_tothread_sig);
LUA54_CHECK_SIG(lua_topointer, lua54_topointer_sig);
LUA54_CHECK_SIG(lua_arith, lua54_arith_sig);
LUA54_CHECK_SIG(lua_rawequal, lua54_rawequal_sig);
LUA54_CHECK_SIG(lua_compare, lua54_compare_sig);
LUA54_CHECK_SIG(lua_pushnil, lua54_pushnil_sig);
LUA54_CHECK_SIG(lua_pushnumber, lua54_pushnumber_sig);
LUA54_CHECK_SIG(lua_pushinteger, lua54_pushinteger_sig);
LUA54_CHECK_SIG(lua_pushlstring, lua54_pushlstring_sig);
LUA54_CHECK_SIG(lua_pushstring, lua54_pushstring_sig);
LUA54_CHECK_SIG(lua_pushvfstring, lua54_pushvfstring_sig);
LUA54_CHECK_SIG(lua_pushfstring, lua54_pushfstring_sig);
LUA54_CHECK_SIG(lua_pushcclosure, lua54_pushcclosure_sig);
LUA54_CHECK_SIG(lua_pushboolean, lua54_pushboolean_sig);
LUA54_CHECK_SIG(lua_pushlightuserdata, lua54_pushlightuserdata_sig);
LUA54_CHECK_SIG(lua_pushthread, lua54_pushthread_sig);
LUA54_CHECK_SIG(lua_getglobal, lua54_getglobal_sig);
LUA54_CHECK_SIG(lua_gettable, lua54_gettable_sig);
LUA54_CHECK_SIG(lua_getfield, lua54_getfield_sig);
LUA54_CHECK_SIG(lua_geti, lua54_geti_sig);
LUA54_CHECK_SIG(lua_rawget, lua54_gettable_sig);
LUA54_CHECK_SIG(lua_rawgeti, lua54_geti_sig);
LUA54_CHECK_SIG(lua_rawgetp, lua54_rawgetp_sig);
LUA54_CHECK_SIG(lua_createtable, lua54_createtable_sig);
LUA54_CHECK_SIG(lua_newuserdatauv, lua54_newuserdatauv_sig);
LUA54_CHECK_SIG(lua_getmetatable, lua54_int_idx_sig);
LUA54_CHECK_SIG(lua_getiuservalue, lua54_getiuservalue_sig);
LUA54_CHECK_SIG(lua_setglobal, lua54_setglobal_sig);
LUA54_CHECK_SIG(lua_settable, lua54_settop_sig);
LUA54_CHECK_SIG(lua_setfield, lua54_setfield_sig);
LUA54_CHECK_SIG(lua_seti, lua54_seti_sig);
LUA54_CHECK_SIG(lua_rawset, lua54_settop_sig);
LUA54_CHECK_SIG(lua_rawseti, lua54_rawseti_sig);
LUA54_CHECK_SIG(lua_rawsetp, lua54_rawsetp_sig);
LUA54_CHECK_SIG(lua_setmetatable, lua54_int_idx_sig);
LUA54_CHECK_SIG(lua_setiuservalue, lua54_setiuservalue_sig);
LUA54_CHECK_SIG(lua_callk, lua54_callk_sig);
LUA54_CHECK_SIG(lua_pcallk, lua54_pcallk_sig);
LUA54_CHECK_SIG(lua_load, lua54_load_sig);
LUA54_CHECK_SIG(lua_dump, lua54_dump_sig);
LUA54_CHECK_SIG(lua_yieldk, lua54_yieldk_sig);
LUA54_CHECK_SIG(lua_resume, lua54_resume_sig);
LUA54_CHECK_SIG(lua_status, lua54_status_sig);
LUA54_CHECK_SIG(lua_isyieldable, lua54_isyieldable_sig);
LUA54_CHECK_SIG(lua_setwarnf, lua54_setwarnf_sig);
LUA54_CHECK_SIG(lua_warning, lua54_warning_sig);
LUA54_CHECK_SIG(lua_gc, lua54_gc_sig);
LUA54_CHECK_SIG(lua_error, lua54_error_sig);
LUA54_CHECK_SIG(lua_next, lua54_next_sig);
LUA54_CHECK_SIG(lua_concat, lua54_concat_sig);
LUA54_CHECK_SIG(lua_len, lua54_len_sig);
LUA54_CHECK_SIG(lua_stringtonumber, lua54_stringtonumber_sig);
LUA54_CHECK_SIG(lua_getallocf, lua54_getallocf_sig);
LUA54_CHECK_SIG(lua_setallocf, lua54_setallocf_sig);
LUA54_CHECK_SIG(lua_toclose, lua54_toclose_sig);
LUA54_CHECK_SIG(lua_closeslot, lua54_closeslot_sig);
LUA54_CHECK_SIG(lua_getstack, lua54_getstack_sig);
LUA54_CHECK_SIG(lua_getinfo, lua54_getinfo_sig);
LUA54_CHECK_SIG(lua_getlocal, lua54_getlocal_sig);
LUA54_CHECK_SIG(lua_setlocal, lua54_getlocal_sig);
LUA54_CHECK_SIG(lua_getupvalue, lua54_getupvalue_sig);
LUA54_CHECK_SIG(lua_setupvalue, lua54_getupvalue_sig);
LUA54_CHECK_SIG(lua_upvalueid, lua54_upvalueid_sig);
LUA54_CHECK_SIG(lua_upvaluejoin, lua54_upvaluejoin_sig);
LUA54_CHECK_SIG(lua_sethook, lua54_sethook_type);
LUA54_CHECK_SIG(lua_gethook, lua54_gethook_sig);
LUA54_CHECK_SIG(lua_gethookmask, lua54_gethookint_sig);
LUA54_CHECK_SIG(lua_gethookcount, lua54_gethookint_sig);
LUA54_CHECK_SIG(lua_setcstacklimit, lua54_setcstacklimit_sig);
#endif

#ifndef LUAI_IS32INT
#error "Lua 5.4 luaconf.h must expose LUAI_IS32INT"
#endif

#ifndef LUA_KCONTEXT
#error "Lua 5.4 luaconf.h must expose LUA_KCONTEXT"
#endif

#ifndef LUA_INT_INT
#error "Lua 5.4 luaconf.h must expose LUA_INT_INT"
#endif

#ifndef LUA_INT_LONG
#error "Lua 5.4 luaconf.h must expose LUA_INT_LONG"
#endif

#ifndef LUA_INT_LONGLONG
#error "Lua 5.4 luaconf.h must expose LUA_INT_LONGLONG"
#endif

#ifndef LUA_FLOAT_FLOAT
#error "Lua 5.4 luaconf.h must expose LUA_FLOAT_FLOAT"
#endif

#ifndef LUA_FLOAT_DOUBLE
#error "Lua 5.4 luaconf.h must expose LUA_FLOAT_DOUBLE"
#endif

#ifndef LUA_FLOAT_LONGDOUBLE
#error "Lua 5.4 luaconf.h must expose LUA_FLOAT_LONGDOUBLE"
#endif

#ifndef LUA_32BITS
#error "Lua 5.4 luaconf.h must expose LUA_32BITS"
#endif

#ifndef LUA_C89_NUMBERS
#error "Lua 5.4 luaconf.h must expose LUA_C89_NUMBERS"
#endif

#ifndef LUA_INT_TYPE
#error "Lua 5.4 luaconf.h must expose LUA_INT_TYPE"
#endif

#ifndef LUA_FLOAT_TYPE
#error "Lua 5.4 luaconf.h must expose LUA_FLOAT_TYPE"
#endif

#if LUA_FLOAT_TYPE != LUA_FLOAT_DOUBLE
#error "This Lua 5.4 compat build currently exposes double Lua numbers"
#endif

#ifndef lua_str2number
#error "Lua 5.4 luaconf.h must expose lua_str2number"
#endif

#ifndef l_floatatt
#error "Lua 5.4 luaconf.h must expose l_floatatt"
#endif

#ifndef lua_pointer2str
#error "Lua 5.4 luaconf.h must expose lua_pointer2str"
#endif

#ifndef lua_getlocaledecpoint
#error "Lua 5.4 luaconf.h must expose lua_getlocaledecpoint"
#endif

#ifndef LUAI_FUNC
#error "Lua 5.4 luaconf.h must expose LUAI_FUNC"
#endif

#ifndef LUAI_DDEC
#error "Lua 5.4 luaconf.h must expose LUAI_DDEC"
#endif

#ifndef LUAI_DDEF
#error "Lua 5.4 luaconf.h must expose LUAI_DDEF"
#endif

#if defined(_WIN32)
#ifndef LUA_USE_WINDOWS
#error "Lua 5.4 luaconf.h must expose LUA_USE_WINDOWS on Windows"
#endif

#ifndef LUA_USE_C89
#error "Lua 5.4 luaconf.h must expose LUA_USE_C89 when LUA_USE_WINDOWS is active"
#endif

#ifndef LUA_DL_DLL
#error "Lua 5.4 luaconf.h must expose LUA_DL_DLL when LUA_USE_WINDOWS is active"
#endif

typedef char lua54_l_mathop_windows_c89_shape[
  sizeof(LUA54_STR(l_mathop(floor))) == sizeof("(lua_Number)floor") ? 1 : -1
];
typedef char lua54_str2number_windows_c89_shape[
  sizeof(LUA54_STR(lua_str2number("1", 0))) ==
  sizeof("((lua_Number)strtod((\"1\"), (0)))") ? 1 : -1
];

#ifdef lua_strx2number
#error "Lua 5.4 luaconf.h must not expose lua_strx2number when LUA_USE_C89 is active"
#endif
#endif

#ifndef luai_likely
#error "Lua 5.4 luaconf.h must expose luai_likely"
#endif

#ifndef luai_unlikely
#error "Lua 5.4 luaconf.h must expose luai_unlikely"
#endif

#ifndef LUA_PATH_SEP
#error "Lua 5.4 luaconf.h must expose LUA_PATH_SEP"
#endif

#ifndef LUA_EXEC_DIR
#error "Lua 5.4 luaconf.h must expose LUA_EXEC_DIR"
#endif

#ifdef LUA_PATHSEP
#error "Lua 5.4 external lua.h must not expose LuaJIT LUA_PATHSEP"
#endif

#ifdef LUA_EXECDIR
#error "Lua 5.4 external lua.h must not expose LuaJIT LUA_EXECDIR"
#endif

#ifdef LUA_PATH
#error "Lua 5.4 external lua.h must not expose internal LUA_PATH env name"
#endif

#ifdef LUA_CPATH
#error "Lua 5.4 external lua.h must not expose internal LUA_CPATH env name"
#endif

#ifdef LUA_INIT
#error "Lua 5.4 external lua.h must not expose internal LUA_INIT env name"
#endif

#ifdef LUA_PATH_5_4
#error "Lua 5.4 external lua.h must not expose internal LUA_PATH_5_4 env name"
#endif

#ifdef LUA_CPATH_5_4
#error "Lua 5.4 external lua.h must not expose internal LUA_CPATH_5_4 env name"
#endif

#ifdef LUA_INIT_5_4
#error "Lua 5.4 external lua.h must not expose internal LUA_INIT_5_4 env name"
#endif

#if defined(__GNUC__) || defined(__clang__)
typedef char lua54_kcontext_type[
  __builtin_types_compatible_p(lua_KContext, LUA_KCONTEXT) ? 1 : -1
];
#if defined(_WIN32)
typedef char lua54_kcontext_windows_official_type[
  __builtin_types_compatible_p(lua_KContext, ptrdiff_t) ? 1 : -1
];
#endif
#endif

typedef char lua54_luai_maxstack_formula[
  LUAI_MAXSTACK == (LUAI_IS32INT ? 1000000 : 15000) ? 1 : -1
];

typedef char lua54_registryindex_formula[
  LUA_REGISTRYINDEX == (-LUAI_MAXSTACK - 1000) ? 1 : -1
];

typedef char lua54_upvalueindex_formula[
  lua_upvalueindex(1) == (LUA_REGISTRYINDEX - 1) ? 1 : -1
];

/* Keep these core constants in the lua.h-only gate: external modules often
** branch on them before including lauxlib.h, so C API smoke alone is too late
** to catch a header-surface regression.
*/
typedef char lua54_signature_shape[
  sizeof(LUA_SIGNATURE) == sizeof("\033Lua") ? 1 : -1
];

typedef char lua54_version_major_shape[
  sizeof(LUA_VERSION_MAJOR) == sizeof("5") ? 1 : -1
];

typedef char lua54_version_minor_shape[
  sizeof(LUA_VERSION_MINOR) == sizeof("4") ? 1 : -1
];

typedef char lua54_version_release_shape[
  sizeof(LUA_VERSION_RELEASE) == sizeof("8") ? 1 : -1
];

typedef char lua54_version_shape[
  sizeof(LUA_VERSION) == sizeof("Lua 5.4") ? 1 : -1
];

typedef char lua54_release_shape[
  sizeof(LUA_RELEASE) == sizeof("Lua 5.4.8") ? 1 : -1
];

typedef char lua54_copyright_shape[
  sizeof(LUA_COPYRIGHT) > sizeof(LUA_RELEASE) ? 1 : -1
];

typedef char lua54_authors_shape[
  sizeof(LUA_AUTHORS) > 1 ? 1 : -1
];

typedef char lua54_version_num_value[
  LUA_VERSION_NUM == 504 ? 1 : -1
];

typedef char lua54_version_release_num_value[
  LUA_VERSION_RELEASE_NUM == 50408 ? 1 : -1
];

typedef char lua54_minstack_value[
  LUA_MINSTACK == 20 ? 1 : -1
];

typedef char lua54_idsize_value[
  LUA_IDSIZE == 60 ? 1 : -1
];

typedef char lua54_ridx_mainthread_value[
  LUA_RIDX_MAINTHREAD == 1 ? 1 : -1
];

typedef char lua54_ridx_globals_value[
  LUA_RIDX_GLOBALS == 2 ? 1 : -1
];

typedef char lua54_ridx_last_formula[
  LUA_RIDX_LAST == LUA_RIDX_GLOBALS ? 1 : -1
];

typedef char lua54_numtypes_value[
  LUA_NUMTYPES == 9 ? 1 : -1
];

typedef char lua54_numtags_formula[
  LUA_NUMTAGS == LUA_NUMTYPES ? 1 : -1
];

typedef char lua54_extraspace_value[
  LUA_EXTRASPACE == sizeof(void *) ? 1 : -1
];

typedef char lua54_multret_value[
  LUA_MULTRET == -1 ? 1 : -1
];

typedef char lua54_status_values[
  LUA_OK == 0 && LUA_YIELD == 1 && LUA_ERRRUN == 2 &&
  LUA_ERRSYNTAX == 3 && LUA_ERRMEM == 4 && LUA_ERRERR == 5 ? 1 : -1
];

typedef char lua54_type_values[
  LUA_TNONE == -1 && LUA_TNIL == 0 && LUA_TBOOLEAN == 1 &&
  LUA_TLIGHTUSERDATA == 2 && LUA_TNUMBER == 3 && LUA_TSTRING == 4 &&
  LUA_TTABLE == 5 && LUA_TFUNCTION == 6 && LUA_TUSERDATA == 7 &&
  LUA_TTHREAD == 8 ? 1 : -1
];

typedef char lua54_arith_op_values[
  LUA_OPADD == 0 && LUA_OPSUB == 1 && LUA_OPMUL == 2 &&
  LUA_OPMOD == 3 && LUA_OPPOW == 4 && LUA_OPDIV == 5 &&
  LUA_OPIDIV == 6 && LUA_OPBAND == 7 && LUA_OPBOR == 8 &&
  LUA_OPBXOR == 9 && LUA_OPSHL == 10 && LUA_OPSHR == 11 &&
  LUA_OPUNM == 12 && LUA_OPBNOT == 13 ? 1 : -1
];

typedef char lua54_compare_op_values[
  LUA_OPEQ == 0 && LUA_OPLT == 1 && LUA_OPLE == 2 ? 1 : -1
];

typedef char lua54_gc_op_values[
  LUA_GCSTOP == 0 && LUA_GCRESTART == 1 && LUA_GCCOLLECT == 2 &&
  LUA_GCCOUNT == 3 && LUA_GCCOUNTB == 4 && LUA_GCSTEP == 5 &&
  LUA_GCSETPAUSE == 6 && LUA_GCSETSTEPMUL == 7 &&
  LUA_GCISRUNNING == 9 && LUA_GCGEN == 10 && LUA_GCINC == 11 ? 1 : -1
];

typedef char lua54_hook_event_values[
  LUA_HOOKCALL == 0 && LUA_HOOKRET == 1 && LUA_HOOKLINE == 2 &&
  LUA_HOOKCOUNT == 3 && LUA_HOOKTAILCALL == 4 ? 1 : -1
];

typedef char lua54_hook_mask_values[
  LUA_MASKCALL == (1 << LUA_HOOKCALL) &&
  LUA_MASKRET == (1 << LUA_HOOKRET) &&
  LUA_MASKLINE == (1 << LUA_HOOKLINE) &&
  LUA_MASKCOUNT == (1 << LUA_HOOKCOUNT) ? 1 : -1
];

#ifdef LUA_LOADED_TABLE
#error "Lua 5.4 lua.h must not expose lauxlib-only LUA_LOADED_TABLE"
#endif

#ifdef LUA_PRELOAD_TABLE
#error "Lua 5.4 lua.h must not expose lauxlib-only LUA_PRELOAD_TABLE"
#endif

#ifdef LUA_HOOKTAILRET
#error "Lua 5.4 lua.h must not expose LuaJIT/Lua 5.1 LUA_HOOKTAILRET"
#endif

#ifdef lua_open
#error "Lua 5.4 lua.h must not expose LuaJIT lua_open"
#endif

#ifdef lua_getregistry
#error "Lua 5.4 lua.h must not expose LuaJIT lua_getregistry"
#endif

#ifdef lua_getgccount
#error "Lua 5.4 lua.h must not expose LuaJIT lua_getgccount"
#endif

#ifdef lua_Chunkreader
#error "Lua 5.4 lua.h must not expose LuaJIT lua_Chunkreader"
#endif

#ifdef lua_Chunkwriter
#error "Lua 5.4 lua.h must not expose LuaJIT lua_Chunkwriter"
#endif

int main(void)
{
  return 0;
}
