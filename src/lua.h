/*
** $Id: lua.h,v 1.218.1.5 2008/08/06 13:30:12 roberto Exp $
** Lua - An Extensible Extension Language
** Lua.org, PUC-Rio, Brazil (https://www.lua.org)
** See Copyright Notice at the end of this file
*/


#ifndef lua_h
#define lua_h

#include <stdarg.h>
#include <stddef.h>


#include "luaconf.h"


#ifdef LUAJIT_ENABLE_LUA54COMPAT
#define LUA_VERSION_MAJOR	"5"
#define LUA_VERSION_MINOR	"4"
#define LUA_VERSION_RELEASE	"8"
#define LUA_VERSION	"Lua 5.4"
#define LUA_RELEASE	"Lua 5.4.8"
#define LUA_VERSION_NUM	504
#define LUA_VERSION_RELEASE_NUM	(LUA_VERSION_NUM * 100 + 8)
#define LUA_COPYRIGHT	LUA_RELEASE "  Copyright (C) 1994-2025 Lua.org, PUC-Rio"
#define LUA_AUTHORS	"R. Ierusalimschy, L. H. de Figueiredo, W. Celes"
#else
#define LUA_VERSION_MAJOR	"5"
#define LUA_VERSION_MINOR	"1"
#define LUA_VERSION_RELEASE	"4"
#define LUA_VERSION	"Lua 5.1"
#define LUA_RELEASE	"Lua 5.1.4"
#define LUA_VERSION_NUM	501
#define LUA_COPYRIGHT	"Copyright (C) 1994-2008 Lua.org, PUC-Rio"
#define LUA_AUTHORS	"R. Ierusalimschy, L. H. de Figueiredo & W. Celes"
#endif

/* Private selector for the external Lua 5.4 header surface. Keep it local to
** this header so extension modules do not observe a non-official macro name.
*/
#undef LUAJIT_EXTERNAL_LUA54
#if defined(LUAJIT_ENABLE_LUA54COMPAT) && !defined(LUA_CORE) && \
    !defined(LUA_LIB) && !defined(LUAJIT_INTERNAL_USE)
#define LUAJIT_EXTERNAL_LUA54 1
#else
#define LUAJIT_EXTERNAL_LUA54 0
#endif


/* mark for precompiled code (`<esc>Lua') */
#define	LUA_SIGNATURE	"\033Lua"

/* option for multiple returns in `lua_pcall' and `lua_call' */
#define LUA_MULTRET	(-1)


/*
** pseudo-indices
*/
#if LUAJIT_EXTERNAL_LUA54
#define LUA_REGISTRYINDEX	(-LUAI_MAXSTACK - 1000)
/* External Lua 5.4 code passes the official pseudo-index range. The API
** normalizes these values to LuaJIT's internal registry/upvalue slots.
*/
#define lua_upvalueindex(i)	(LUA_REGISTRYINDEX-(i))
#else
#define LUA_REGISTRYINDEX	(-10000)
#define LUA_ENVIRONINDEX	(-10001)
#define LUA_GLOBALSINDEX	(-10002)
#define lua_upvalueindex(i)	(LUA_GLOBALSINDEX-(i))
#endif

#define LUA_RIDX_MAINTHREAD	1
#define LUA_RIDX_GLOBALS	2
#define LUA_RIDX_LAST		LUA_RIDX_GLOBALS
#if !LUAJIT_EXTERNAL_LUA54
#define LUA_LOADED_TABLE	"_LOADED"
#define LUA_PRELOAD_TABLE	"_PRELOAD"
#endif


/* thread status */
#define LUA_OK		0
#define LUA_YIELD	1
#define LUA_ERRRUN	2
#define LUA_ERRSYNTAX	3
#define LUA_ERRMEM	4
#define LUA_ERRERR	5


typedef struct lua_State lua_State;

typedef int (*lua_CFunction) (lua_State *L);
#ifdef LUAJIT_ENABLE_LUA54COMPAT
typedef LUA_KCONTEXT lua_KContext;
#else
typedef ptrdiff_t lua_KContext;
#endif
typedef int (*lua_KFunction) (lua_State *L, int status, lua_KContext ctx);
typedef void (*lua_WarnFunction) (void *ud, const char *msg, int tocont);


/*
** functions that read/write blocks when loading/dumping Lua chunks
*/
typedef const char * (*lua_Reader) (lua_State *L, void *ud, size_t *sz);

typedef int (*lua_Writer) (lua_State *L, const void* p, size_t sz, void* ud);


/*
** prototype for memory-allocation functions
*/
typedef void * (*lua_Alloc) (void *ud, void *ptr, size_t osize, size_t nsize);


/*
** basic types
*/
#define LUA_TNONE		(-1)

#define LUA_TNIL		0
#define LUA_TBOOLEAN		1
#define LUA_TLIGHTUSERDATA	2
#define LUA_TNUMBER		3
#define LUA_TSTRING		4
#define LUA_TTABLE		5
#define LUA_TFUNCTION		6
#define LUA_TUSERDATA		7
#define LUA_TTHREAD		8
#define LUA_NUMTYPES		9
#ifdef LUAJIT_ENABLE_LUA54COMPAT
#define LUA_NUMTAGS		LUA_NUMTYPES
#endif



/* minimum Lua stack available to a C function */
#define LUA_MINSTACK	20


/*
** generic extra include file
*/
#if defined(LUA_USER_H)
#include LUA_USER_H
#endif

#ifdef LUAJIT_ENABLE_LUA54COMPAT
extern const char lua_ident[];
#endif


/* type of numbers in Lua */
typedef LUA_NUMBER lua_Number;


/* type for integer functions */
typedef LUA_INTEGER lua_Integer;
typedef LUA_UNSIGNED lua_Unsigned;

#if LUAJIT_EXTERNAL_LUA54
#define LUA_MAXINTEGER	((lua_Integer)(((lua_Unsigned)~(lua_Unsigned)0) >> 1))
#else
#define LUA_MAXINTEGER	((lua_Integer)2147483647)
#endif
#define LUA_MININTEGER	((lua_Integer)(-LUA_MAXINTEGER - 1))



/*
** state manipulation
*/
LUA_API lua_State *(lua_newstate) (lua_Alloc f, void *ud);
LUA_API void       (lua_close) (lua_State *L);
#ifdef LUAJIT_ENABLE_LUA54COMPAT
LUA_API void       (lua_closeslot) (lua_State *L, int idx);
#endif
LUA_API lua_State *(lua_newthread) (lua_State *L);

LUA_API lua_CFunction (lua_atpanic) (lua_State *L, lua_CFunction panicf);
#if LUAJIT_EXTERNAL_LUA54
LUA_API void       *(lua_getextraspace54) (lua_State *L);
#define lua_getextraspace(L)	lua_getextraspace54((L))
#else
LUA_API void       *(lua_getextraspace) (lua_State *L);
#endif


/*
** basic stack manipulation
*/
LUA_API int   (lua_gettop) (lua_State *L);
LUA_API void  (lua_settop) (lua_State *L, int idx);
LUA_API void  (lua_pushvalue) (lua_State *L, int idx);
#if !LUAJIT_EXTERNAL_LUA54
LUA_API void  (lua_remove) (lua_State *L, int idx);
LUA_API void  (lua_insert) (lua_State *L, int idx);
#endif
LUA_API void  (lua_rotate) (lua_State *L, int idx, int n);
#ifdef LUAJIT_ENABLE_LUA54COMPAT
LUA_API void  (lua_copy) (lua_State *L, int fromidx, int toidx);
#endif
#if !LUAJIT_EXTERNAL_LUA54
LUA_API void  (lua_replace) (lua_State *L, int idx);
#endif
LUA_API int   (lua_checkstack) (lua_State *L, int sz);
#ifdef LUAJIT_ENABLE_LUA54COMPAT
LUA_API int   (lua_setcstacklimit) (lua_State *L, unsigned int limit);
#endif

LUA_API void  (lua_xmove) (lua_State *from, lua_State *to, int n);


/*
** access functions (stack -> C)
*/

LUA_API int             (lua_isnumber) (lua_State *L, int idx);
LUA_API int             (lua_isstring) (lua_State *L, int idx);
LUA_API int             (lua_iscfunction) (lua_State *L, int idx);
LUA_API int             (lua_isinteger) (lua_State *L, int idx);
LUA_API int             (lua_isuserdata) (lua_State *L, int idx);
LUA_API int             (lua_type) (lua_State *L, int idx);
LUA_API const char     *(lua_typename) (lua_State *L, int tp);

LUA_API int            (lua_rawequal) (lua_State *L, int idx1, int idx2);
LUA_API int            (lua_compare) (lua_State *L, int idx1, int idx2, int op);
#if !LUAJIT_EXTERNAL_LUA54
LUA_API int            (lua_equal) (lua_State *L, int idx1, int idx2);
LUA_API int            (lua_lessthan) (lua_State *L, int idx1, int idx2);
#endif

#if !LUAJIT_EXTERNAL_LUA54
LUA_API lua_Number      (lua_tonumber) (lua_State *L, int idx);
LUA_API lua_Integer     (lua_tointeger) (lua_State *L, int idx);
#endif
LUA_API int             (lua_toboolean) (lua_State *L, int idx);
LUA_API const char     *(lua_tolstring) (lua_State *L, int idx, size_t *len);
#if LUAJIT_EXTERNAL_LUA54
LUA_API lua_Unsigned    (lua_rawlen54) (lua_State *L, int idx);
#define lua_rawlen	lua_rawlen54
#else
LUA_API size_t          (lua_objlen) (lua_State *L, int idx);
LUA_API size_t          (lua_rawlen) (lua_State *L, int idx);
#endif
LUA_API void            (lua_len) (lua_State *L, int idx);
LUA_API lua_CFunction   (lua_tocfunction) (lua_State *L, int idx);
#ifdef LUAJIT_ENABLE_LUA54COMPAT
LUA_API void            (lua_toclose) (lua_State *L, int idx);
#endif
LUA_API void	       *(lua_touserdata) (lua_State *L, int idx);
LUA_API lua_State      *(lua_tothread) (lua_State *L, int idx);
LUA_API const void     *(lua_topointer) (lua_State *L, int idx);


/*
** push functions (C -> stack)
*/
LUA_API void  (lua_pushnil) (lua_State *L);
LUA_API void  (lua_pushnumber) (lua_State *L, lua_Number n);
LUA_API void  (lua_pushinteger) (lua_State *L, lua_Integer n);
#if LUAJIT_EXTERNAL_LUA54
LUA_API const char *(lua_pushlstring54) (lua_State *L, const char *s,
					 size_t l);
LUA_API const char *(lua_pushstring54) (lua_State *L, const char *s);
#define lua_pushlstring	lua_pushlstring54
#define lua_pushstring	lua_pushstring54
#else
LUA_API void  (lua_pushlstring) (lua_State *L, const char *s, size_t l);
LUA_API void  (lua_pushstring) (lua_State *L, const char *s);
#endif
LUA_API const char *(lua_pushvfstring) (lua_State *L, const char *fmt,
						       va_list argp);
LUA_API const char *(lua_pushfstring) (lua_State *L, const char *fmt, ...);
LUA_API void  (lua_pushcclosure) (lua_State *L, lua_CFunction fn, int n);
LUA_API void  (lua_pushboolean) (lua_State *L, int b);
LUA_API void  (lua_pushlightuserdata) (lua_State *L, void *p);
LUA_API int   (lua_pushthread) (lua_State *L);


/*
** get functions (Lua -> stack)
*/
#if LUAJIT_EXTERNAL_LUA54
LUA_API int   (lua_gettable54) (lua_State *L, int idx);
LUA_API int   (lua_getfield54) (lua_State *L, int idx, const char *k);
LUA_API int   (lua_geti54) (lua_State *L, int idx, lua_Integer n);
LUA_API int   (lua_rawget54) (lua_State *L, int idx);
LUA_API int   (lua_rawgeti54) (lua_State *L, int idx, lua_Integer n);
LUA_API int   (lua_rawgetp54) (lua_State *L, int idx, const void *p);
#define lua_gettable	lua_gettable54
#define lua_getfield	lua_getfield54
#define lua_geti	lua_geti54
#define lua_rawget	lua_rawget54
#define lua_rawgeti	lua_rawgeti54
#define lua_rawgetp	lua_rawgetp54
#else
LUA_API void  (lua_gettable) (lua_State *L, int idx);
LUA_API void  (lua_getfield) (lua_State *L, int idx, const char *k);
LUA_API void  (lua_geti) (lua_State *L, int idx, lua_Integer n);
LUA_API void  (lua_rawget) (lua_State *L, int idx);
LUA_API void  (lua_rawgeti) (lua_State *L, int idx, int n);
LUA_API void  (lua_rawgetp) (lua_State *L, int idx, const void *p);
#endif
LUA_API void  (lua_createtable) (lua_State *L, int narr, int nrec);
#if LUAJIT_EXTERNAL_LUA54
/* Lua 5.4 keeps the old single-uservalue helpers as aliases for slot 1. */
#define lua_newuserdata(L,s)	lua_newuserdatauv((L), (s), 1)
#else
LUA_API void *(lua_newuserdata) (lua_State *L, size_t sz);
#endif
LUA_API void *(lua_newuserdatauv) (lua_State *L, size_t sz, int nuvalue);
LUA_API int   (lua_getmetatable) (lua_State *L, int objindex);
#if !LUAJIT_EXTERNAL_LUA54
LUA_API void  (lua_getfenv) (lua_State *L, int idx);
#endif
LUA_API int   (lua_getiuservalue) (lua_State *L, int idx, int n);
#if LUAJIT_EXTERNAL_LUA54
#define lua_getuservalue(L,idx)	lua_getiuservalue((L), (idx), 1)
#endif


/*
** set functions (stack -> Lua)
*/
LUA_API void  (lua_settable) (lua_State *L, int idx);
LUA_API void  (lua_setfield) (lua_State *L, int idx, const char *k);
LUA_API void  (lua_seti) (lua_State *L, int idx, lua_Integer n);
LUA_API void  (lua_rawset) (lua_State *L, int idx);
#if LUAJIT_EXTERNAL_LUA54
LUA_API void  (lua_rawseti54) (lua_State *L, int idx, lua_Integer n);
#define lua_rawseti	lua_rawseti54
#else
LUA_API void  (lua_rawseti) (lua_State *L, int idx, int n);
#endif
LUA_API void  (lua_rawsetp) (lua_State *L, int idx, const void *p);
LUA_API int   (lua_setmetatable) (lua_State *L, int objindex);
#if !LUAJIT_EXTERNAL_LUA54
LUA_API int   (lua_setfenv) (lua_State *L, int idx);
#endif
LUA_API int   (lua_setiuservalue) (lua_State *L, int idx, int n);
#if LUAJIT_EXTERNAL_LUA54
#define lua_setuservalue(L,idx)	lua_setiuservalue((L), (idx), 1)
#endif


/*
** `load' and `call' functions (load and run Lua code)
*/
#if !LUAJIT_EXTERNAL_LUA54
LUA_API void  (lua_call) (lua_State *L, int nargs, int nresults);
LUA_API int   (lua_pcall) (lua_State *L, int nargs, int nresults, int errfunc);
#endif
#ifdef LUAJIT_ENABLE_LUA54COMPAT
LUA_API void  (lua_callk) (lua_State *L, int nargs, int nresults,
			   lua_KContext ctx, lua_KFunction k);
LUA_API int   (lua_pcallk) (lua_State *L, int nargs, int nresults,
			    int errfunc, lua_KContext ctx, lua_KFunction k);
#endif
#if !LUAJIT_EXTERNAL_LUA54
LUA_API int   (lua_cpcall) (lua_State *L, lua_CFunction func, void *ud);
#endif
#if LUAJIT_EXTERNAL_LUA54
LUA_API int   (lua_load54) (lua_State *L, lua_Reader reader, void *dt,
                                        const char *chunkname,
                                        const char *mode);
#define lua_load	lua_load54
#else
LUA_API int   (lua_load) (lua_State *L, lua_Reader reader, void *dt,
                                        const char *chunkname);
#endif

#if LUAJIT_EXTERNAL_LUA54
LUA_API int (lua_dump54) (lua_State *L, lua_Writer writer, void *data,
			  int strip);
#define lua_dump	lua_dump54
#else
LUA_API int (lua_dump) (lua_State *L, lua_Writer writer, void *data);
#endif


/*
** coroutine functions
*/
#if !LUAJIT_EXTERNAL_LUA54
LUA_API int  (lua_yield) (lua_State *L, int nresults);
#endif
#ifdef LUAJIT_ENABLE_LUA54COMPAT
LUA_API int  (lua_yieldk) (lua_State *L, int nresults, lua_KContext ctx,
			   lua_KFunction k);
LUA_API int  (lua_resume54) (lua_State *L, lua_State *from, int nargs,
			     int *nresults);
#if LUAJIT_EXTERNAL_LUA54
/* LuaJIT keeps the legacy 2-argument ABI internally; external Lua 5.4
** compatibility headers expose the official 4-argument resume surface.
*/
#define lua_resume	lua_resume54
#endif
#else
LUA_API int  (lua_resume) (lua_State *L, int narg);
#endif
LUA_API int  (lua_resetthread) (lua_State *L);
#ifdef LUAJIT_ENABLE_LUA54COMPAT
LUA_API int  (lua_closethread) (lua_State *L, lua_State *from);
#endif
LUA_API int  (lua_status) (lua_State *L);

/*
** garbage-collection function and options
*/

#define LUA_GCSTOP		0
#define LUA_GCRESTART		1
#define LUA_GCCOLLECT		2
#define LUA_GCCOUNT		3
#define LUA_GCCOUNTB		4
#define LUA_GCSTEP		5
#define LUA_GCSETPAUSE		6
#define LUA_GCSETSTEPMUL	7
#define LUA_GCISRUNNING		9
#define LUA_GCGEN		10
#define LUA_GCINC		11

#ifdef LUAJIT_ENABLE_LUA54COMPAT
LUA_API int (lua_gc) (lua_State *L, int what, ...);
#else
LUA_API int (lua_gc) (lua_State *L, int what, int data);
#endif


/*
** miscellaneous functions
*/

LUA_API int   (lua_error) (lua_State *L);

LUA_API int   (lua_absindex) (lua_State *L, int idx);

LUA_API void  (lua_arith) (lua_State *L, int op);

LUA_API int   (lua_next) (lua_State *L, int idx);

LUA_API void  (lua_concat) (lua_State *L, int n);

LUA_API lua_Alloc (lua_getallocf) (lua_State *L, void **ud);
LUA_API void lua_setallocf (lua_State *L, lua_Alloc f, void *ud);
LUA_API size_t lua_stringtonumber (lua_State *L, const char *s);
LUA_API void lua_setwarnf (lua_State *L, lua_WarnFunction f, void *ud);
LUA_API void lua_warning (lua_State *L, const char *msg, int tocont);
#if LUAJIT_EXTERNAL_LUA54
LUA_API int lua_getglobal54 (lua_State *L, const char *name);
LUA_API void lua_setglobal54 (lua_State *L, const char *name);
#endif



/*
** ===============================================================
** some useful macros
** ===============================================================
*/

#define lua_pop(L,n)		lua_settop(L, -(n)-1)

#define lua_newtable(L)		lua_createtable(L, 0, 0)

#define lua_register(L,n,f) (lua_pushcfunction(L, (f)), lua_setglobal(L, (n)))

#define lua_pushcfunction(L,f)	lua_pushcclosure(L, (f), 0)

#if !LUAJIT_EXTERNAL_LUA54
#define lua_strlen(L,i)		lua_objlen(L, (i))
#elif defined(LUA_COMPAT_5_3)
#define lua_strlen(L,i)		lua_rawlen((L), (i))
#define lua_objlen(L,i)		lua_rawlen((L), (i))
#define lua_equal(L,idx1,idx2)	lua_compare((L), (idx1), (idx2), LUA_OPEQ)
#define lua_lessthan(L,idx1,idx2) \
  lua_compare((L), (idx1), (idx2), LUA_OPLT)
#endif

#define lua_isfunction(L,n)	(lua_type(L, (n)) == LUA_TFUNCTION)
#define lua_istable(L,n)	(lua_type(L, (n)) == LUA_TTABLE)
#define lua_islightuserdata(L,n)	(lua_type(L, (n)) == LUA_TLIGHTUSERDATA)
#define lua_isnil(L,n)		(lua_type(L, (n)) == LUA_TNIL)
#define lua_isboolean(L,n)	(lua_type(L, (n)) == LUA_TBOOLEAN)
#define lua_isthread(L,n)	(lua_type(L, (n)) == LUA_TTHREAD)
#define lua_isnone(L,n)		(lua_type(L, (n)) == LUA_TNONE)
#define lua_isnoneornil(L, n)	(lua_type(L, (n)) <= 0)

#if LUAJIT_EXTERNAL_LUA54
#define lua_pushliteral(L, s)	lua_pushstring((L), "" s)
#else
#define lua_pushliteral(L, s)	\
	lua_pushlstring(L, "" s, (sizeof(s)/sizeof(char))-1)
#endif

#if LUAJIT_EXTERNAL_LUA54
#define lua_pushglobaltable(L)	((void)lua_rawgeti((L), LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS))
#define lua_setglobal	lua_setglobal54
#define lua_getglobal	lua_getglobal54
#else
#define lua_pushglobaltable(L)	lua_pushvalue(L, LUA_GLOBALSINDEX)
#define lua_setglobal(L,s)	lua_setfield(L, LUA_GLOBALSINDEX, (s))
#define lua_getglobal(L,s)	lua_getfield(L, LUA_GLOBALSINDEX, (s))
#endif

#define lua_tostring(L,i)	lua_tolstring(L, (i), NULL)

#if LUAJIT_EXTERNAL_LUA54
#define lua_tonumber(L,i)	lua_tonumberx((L), (i), NULL)
#define lua_tointeger(L,i)	lua_tointegerx((L), (i), NULL)
#define lua_insert(L,idx)	lua_rotate((L), (idx), 1)
#define lua_remove(L,idx)	(lua_rotate((L), (idx), -1), lua_pop((L), 1))
#define lua_replace(L,idx)	(lua_copy((L), -1, (idx)), lua_pop((L), 1))
#endif

#if defined(LUAJIT_ENABLE_LUA54COMPAT) && defined(LUA_COMPAT_APIINTCASTS)
#define lua_pushunsigned(L,n)	lua_pushinteger(L, (lua_Integer)(n))
#define lua_tounsignedx(L,i,is)	((lua_Unsigned)lua_tointegerx(L, (i), (is)))
#define lua_tounsigned(L,i)	lua_tounsignedx(L, (i), NULL)
#endif

#if LUAJIT_EXTERNAL_LUA54
#define lua_call(L,n,r)	lua_callk((L), (n), (r), 0, NULL)
#define lua_pcall(L,n,r,e)	lua_pcallk((L), (n), (r), (e), 0, NULL)
#define lua_yield(L,n)	lua_yieldk((L), (n), 0, NULL)
#else
#define lua_callk(L,n,r,ctx,k) \
  ((void)(ctx), (void)(k), lua_call((L), (n), (r)))
#define lua_pcallk(L,n,r,e,ctx,k) \
  ((void)(ctx), (void)(k), lua_pcall((L), (n), (r), (e)))
#define lua_yieldk(L,n,ctx,k) \
  ((void)(ctx), (void)(k), lua_yield((L), (n)))
#endif

#define lua_numbertointeger(n,p) \
  ((n) >= (lua_Number)LUA_MININTEGER && \
   (n) < -(lua_Number)LUA_MININTEGER && \
   (*(p) = (lua_Integer)(n), 1))

#define LUA_OPEQ	0
#define LUA_OPLT	1
#define LUA_OPLE	2

#define LUA_OPADD	0
#define LUA_OPSUB	1
#define LUA_OPMUL	2
#define LUA_OPMOD	3
#define LUA_OPPOW	4
#define LUA_OPDIV	5
#define LUA_OPIDIV	6
#define LUA_OPBAND	7
#define LUA_OPBOR	8
#define LUA_OPBXOR	9
#define LUA_OPSHL	10
#define LUA_OPSHR	11
#define LUA_OPUNM	12
#define LUA_OPBNOT	13



/*
** compatibility macros and functions
*/

#if !LUAJIT_EXTERNAL_LUA54
#define lua_open()	luaL_newstate()

#define lua_getregistry(L)	lua_pushvalue(L, LUA_REGISTRYINDEX)

#define lua_getgccount(L)	lua_gc(L, LUA_GCCOUNT, 0)

#define lua_Chunkreader		lua_Reader
#define lua_Chunkwriter		lua_Writer


/* hack */
LUA_API void lua_setlevel	(lua_State *from, lua_State *to);
#endif


/*
** {======================================================================
** Debug API
** =======================================================================
*/


/*
** Event codes
*/
#define LUA_HOOKCALL	0
#define LUA_HOOKRET	1
#define LUA_HOOKLINE	2
#define LUA_HOOKCOUNT	3
#if LUAJIT_EXTERNAL_LUA54
#define LUA_HOOKTAILCALL 4
#else
#define LUA_HOOKTAILRET 4
#define LUA_HOOKTAILCALL LUA_HOOKTAILRET
#endif


/*
** Event masks
*/
#define LUA_MASKCALL	(1 << LUA_HOOKCALL)
#define LUA_MASKRET	(1 << LUA_HOOKRET)
#define LUA_MASKLINE	(1 << LUA_HOOKLINE)
#define LUA_MASKCOUNT	(1 << LUA_HOOKCOUNT)

typedef struct lua_Debug lua_Debug;  /* activation record */


/* Functions to be called by the debuger in specific events */
typedef void (*lua_Hook) (lua_State *L, lua_Debug *ar);


LUA_API int lua_getstack (lua_State *L, int level, lua_Debug *ar);
LUA_API int lua_getinfo (lua_State *L, const char *what, lua_Debug *ar);
LUA_API const char *lua_getlocal (lua_State *L, const lua_Debug *ar, int n);
LUA_API const char *lua_setlocal (lua_State *L, const lua_Debug *ar, int n);
LUA_API const char *lua_getupvalue (lua_State *L, int funcindex, int n);
LUA_API const char *lua_setupvalue (lua_State *L, int funcindex, int n);
#if LUAJIT_EXTERNAL_LUA54
/* Lua 5.4 changed lua_sethook to void; keep LuaJIT's internal int ABI and
** expose the official signature through an external compatibility wrapper.
*/
LUA_API void lua_sethook54 (lua_State *L, lua_Hook func, int mask, int count);
#define lua_sethook	lua_sethook54
#else
LUA_API int lua_sethook (lua_State *L, lua_Hook func, int mask, int count);
#endif
LUA_API lua_Hook lua_gethook (lua_State *L);
LUA_API int lua_gethookmask (lua_State *L);
LUA_API int lua_gethookcount (lua_State *L);

/* From Lua 5.2. */
LUA_API void *lua_upvalueid (lua_State *L, int idx, int n);
LUA_API void lua_upvaluejoin (lua_State *L, int idx1, int n1, int idx2, int n2);
#if !LUAJIT_EXTERNAL_LUA54
LUA_API int lua_loadx (lua_State *L, lua_Reader reader, void *dt,
		       const char *chunkname, const char *mode);
#endif
#if LUAJIT_EXTERNAL_LUA54
LUA_API lua_Number lua_version54 (lua_State *L);
#define lua_version	lua_version54
#else
LUA_API const lua_Number *lua_version (lua_State *L);
#endif
LUA_API void lua_copy (lua_State *L, int fromidx, int toidx);
LUA_API lua_Number lua_tonumberx (lua_State *L, int idx, int *isnum);
LUA_API lua_Integer lua_tointegerx (lua_State *L, int idx, int *isnum);

/* From Lua 5.3. */
LUA_API int lua_isyieldable (lua_State *L);


struct lua_Debug {
  int event;
  const char *name;	/* (n) */
  const char *namewhat;	/* (n) `global', `local', `field', `method' */
  const char *what;	/* (S) `Lua', `C', `main', `tail' */
  const char *source;	/* (S) */
#ifdef LUAJIT_ENABLE_LUA54COMPAT
  size_t srclen;	/* (S) */
  int currentline;	/* (l) */
  int linedefined;	/* (S) */
  int lastlinedefined;	/* (S) */
  unsigned char nups;	/* (u) number of upvalues */
  unsigned char nparams;/* (u) number of parameters */
  char isvararg;	/* (u) */
  char istailcall;	/* (t) */
  unsigned short ftransfer;	/* (r) index of first value transferred */
  unsigned short ntransfer;	/* (r) number of transferred values */
  char short_src[LUA_IDSIZE]; /* (S) */
  /* private part: LuaJIT stores an encoded frame id in this pointer-sized slot. */
  struct CallInfo *i_ci;
#else
  int currentline;	/* (l) */
  int nups;		/* (u) number of upvalues */
  int linedefined;	/* (S) */
  int lastlinedefined;	/* (S) */
  char short_src[LUA_IDSIZE]; /* (S) */
  int nparams;		/* (u) number of parameters */
  int isvararg;		/* (u) */
  int istailcall;	/* (t) */
  unsigned short ftransfer;	/* (r) index of first value transferred */
  unsigned short ntransfer;	/* (r) number of transferred values */
  /* private part */
  int i_ci;  /* active function */
#endif
};

/* }====================================================================== */

#undef LUAJIT_EXTERNAL_LUA54


/******************************************************************************
* Copyright (C) 1994-2008 Lua.org, PUC-Rio.  All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be
* included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
******************************************************************************/


#endif
