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
#endif

#endif
