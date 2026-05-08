/*
** Miscellaneous object handling.
** Copyright (C) 2005-2026 Mike Pall. See Copyright Notice in luajit.h
*/

#define lj_obj_c
#define LUA_CORE

#include "lj_obj.h"
#include "lj_str.h"

/* Object type names. */
LJ_DATADEF const char *const lj_obj_typename[] = {  /* ORDER LUA_T */
  "no value", "nil", "boolean", "userdata", "number", "string",
  "table", "function", "userdata", "thread", "proto", "cdata"
};

LJ_DATADEF const char *const lj_obj_itypename[] = {  /* ORDER LJ_T */
  "nil", "boolean", "boolean", "userdata", "string", "upval", "thread",
  "proto", "function", "trace", "cdata", "table", "userdata", "number"
};

/* Compare two objects without calling metamethods. */
int LJ_FASTCALL lj_obj_equal(cTValue *o1, cTValue *o2)
{
  if (itype(o1) == itype(o2)) {
    if (tvispri(o1))
      return 1;
    if (tvisstr(o1)) {
      GCstr *s1 = strV(o1);
      GCstr *s2 = strV(o2);
      /* Lua 5.4 keeps long strings as separate objects, so raw string
      ** equality cannot rely only on pointer identity in compatibility mode.
      */
      return lj_str_equal(s1, s2);
    }
    if (!tvisnum(o1))
      return gcrefeq(o1->gcr, o2->gcr);
  } else if (!tvisnumber(o1) || !tvisnumber(o2)) {
    return 0;
  }
  return numberVnum(o1) == numberVnum(o2);
}

/* Return pointer to object or its object data. */
const void * LJ_FASTCALL lj_obj_ptr(global_State *g, cTValue *o)
{
  UNUSED(g);
  if (tvisudata(o))
    return uddata(udataV(o));
  else if (tvislightud(o))
    return lightudV(g, o);
  else if (LJ_HASFFI && tviscdata(o))
    return cdataptr(cdataV(o));
  else if (tvisgcv(o))
    return gcV(o);
  else
    return NULL;
}

