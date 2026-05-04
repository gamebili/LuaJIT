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
LJ_FUNC void lj_close_mark(lua_State *L, TValue *slot);
LJ_FUNC void lj_close_unmark(lua_State *L, TValue *slot);
LJ_FUNC int lj_close_unwind(lua_State *L, TValue *level);
LJ_FUNC int lj_close_unwind_status(lua_State *L, TValue *level, int status);
LJ_FUNC uint32_t lj_close_cframe(lua_State *L, uint32_t nres1);
LJ_FUNC void lj_close_freeall(lua_State *L);
#endif

#endif
