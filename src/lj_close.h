/*
** Lua 5.4 to-be-closed value helpers.
** Copyright (C) 2005-2026 Mike Pall. See Copyright Notice in luajit.h
*/

#ifndef _LJ_CLOSE_H
#define _LJ_CLOSE_H

#include "lj_obj.h"

#if LJ_54
LJ_FUNC int lj_close_isfalse(cTValue *o);
LJ_FUNC cTValue *lj_close_getmethod(lua_State *L, cTValue *o);
LJ_FUNC int lj_close_check(lua_State *L, cTValue *o);
LJ_FUNC int lj_close_call(lua_State *L, TValue *slot, cTValue *err, int clear);
LJ_FUNC int lj_close_canmark(lua_State *L, TValue *slot);
LJ_FUNC int lj_close_islast(lua_State *L, TValue *slot);
LJ_FUNC void lj_close_mark(lua_State *L, TValue *slot);
LJ_FUNC void lj_close_unmark(lua_State *L, TValue *slot);
LJ_FUNC int lj_close_hasunwind(lua_State *L, TValue *level);
LJ_FUNC int lj_close_unwind(lua_State *L, TValue *level);
LJ_FUNC int lj_close_unwind_status(lua_State *L, TValue *level, int status);
LJ_FUNC int lj_close_unwind_pcall(lua_State *L);
#if LJ_TARGET_ARM || LJ_TARGET_MIPS || LJ_TARGET_MIPS64 || LJ_TARGET_PPC || LJ_TARGET_X86 || LJ_TARGET_X64 || LJ_TARGET_ARM64
LJ_FUNC TValue *lj_close_prepare_pcall(lua_State *L);
LJ_FUNC TValue *lj_close_continue_pcall(lua_State *L, TValue *mbase,
					TValue *res, int nres1);
LJ_FUNC TValue *lj_close_prepare_cframe_pcall(lua_State *L, uint32_t nres1);
LJ_FUNC TValue *lj_close_continue_cframe_pcall(lua_State *L, TValue *mbase,
					       TValue *res, int nres1);
LJ_FUNC TValue *lj_close_prepare_return_pcall(lua_State *L, TValue *res,
					      uint32_t nres1,
					      uint32_t multres);
LJ_FUNC TValue *lj_close_prepare_return_hook_pcall(lua_State *L, TValue *res,
						   uint32_t nres1,
						   uint32_t multres);
LJ_FUNC TValue *lj_close_continue_return_pcall(lua_State *L, TValue *mbase,
					       TValue *res, int nres1);
LJ_FUNC TValue *lj_close_continue_return_hook_pcall(lua_State *L,
						    TValue *mbase,
						    TValue *res, int nres1);
#endif
LJ_FUNC uint32_t lj_close_cframe(lua_State *L, uint32_t nres1);
LJ_FUNC void lj_close_freeall(lua_State *L);
#endif

#endif
