/*
** Miscellaneous object handling.
** Copyright (C) 2005-2026 Mike Pall. See Copyright Notice in luajit.h
*/

#define lj_obj_c
#define LUA_CORE

#include <math.h>

#include "lj_obj.h"
#include "lj_gc.h"
#include "lj_str.h"
#include "lj_vm.h"

/* Object type names. */
LJ_DATADEF const char *const lj_obj_typename[] = {  /* ORDER LUA_T */
  "no value", "nil", "boolean", "userdata", "number", "string",
  "table", "function", "userdata", "thread", "proto", "cdata"
};

LJ_DATADEF const char *const lj_obj_itypename[] = {  /* ORDER LJ_T */
  "nil", "boolean", "boolean", "userdata", "string", "upval", "thread",
  "proto", "function", "trace", "cdata", "int64", "table", "userdata",
  "number"
};

GCint64 *lj_obj_newint64(lua_State *L, int64_t i)
{
  GCint64 *i64 = (GCint64 *)lj_mem_newgco(L, sizeof(GCint64));
  i64->gct = ~LJ_TINT64;
  i64->i = i;
  return i64;
}

void LJ_FASTCALL lj_obj_freeint64(global_State *g, GCint64 *i64)
{
  lj_mem_freet(g, i64);
}

void lj_obj_setint64(lua_State *L, TValue *o, int64_t i)
{
  if (LJ_DUALNUM && LJ_LIKELY(i == (int64_t)(int32_t)i)) {
    setintV(o, (int32_t)i);
  } else {
    GCint64 *i64;
    lj_gc_check(L);
    i64 = lj_obj_newint64(L, i);
    seti64V(L, o, i64);
  }
}

static int obj_integer(cTValue *o, int64_t *ip)
{
  if (tvisint(o)) {
    *ip = (int64_t)intV(o);
    return 1;
  } else if (tvisi64(o)) {
    *ip = i64V(o);
    return 1;
  }
  return 0;
}

static int obj_numeq_i64(lua_Number n, int64_t i)
{
  int64_t k;
  if (!(n >= (-9223372036854775807.0 - 1.0) && n < 9223372036854775808.0))
    return 0;
  k = lj_num2i64(n);
  return k == i && (lua_Number)k == n;
}

int lj_obj_i64eqnum(int64_t i, lua_Number n)
{
  return obj_numeq_i64(n, i);
}

static int obj_i64lt_num(int64_t i, lua_Number n)
{
  lua_Number nf;
  int64_t k;
  if (!(n == n))
    return 0;
  if (n <= (-9223372036854775807.0 - 1.0))
    return 0;
  if (n >= 9223372036854775808.0)
    return 1;
  nf = lj_vm_floor(n);
  k = lj_num2i64(nf);
  return i < k || (i == k && nf < n);
}

static int obj_numlt_i64(lua_Number n, int64_t i)
{
  lua_Number nf;
  int64_t k;
  if (!(n == n))
    return 0;
  if (n < (-9223372036854775807.0 - 1.0))
    return 1;
  if (n == (-9223372036854775807.0 - 1.0))
    return i > (-9223372036854775807LL - 1LL);
  if (n >= 9223372036854775808.0)
    return 0;
  nf = lj_vm_floor(n);
  k = lj_num2i64(nf);
  return k < i;
}

int lj_obj_i64cmpnum(int64_t i, lua_Number n, int op)
{
  switch (op) {
  case LJ_OBJ_CMPLT: return obj_i64lt_num(i, n);
  case LJ_OBJ_CMPGE: return obj_numeq_i64(n, i) || obj_numlt_i64(n, i);
  case LJ_OBJ_CMPLE: return obj_numeq_i64(n, i) || obj_i64lt_num(i, n);
  case LJ_OBJ_CMPGT: return obj_numlt_i64(n, i);
  default: return 0;
  }
}

int lj_obj_numcmpi64(lua_Number n, int64_t i, int op)
{
  switch (op) {
  case LJ_OBJ_CMPLT: return obj_numlt_i64(n, i);
  case LJ_OBJ_CMPGE: return obj_numeq_i64(n, i) || obj_i64lt_num(i, n);
  case LJ_OBJ_CMPLE: return obj_numeq_i64(n, i) || obj_numlt_i64(n, i);
  case LJ_OBJ_CMPGT: return obj_i64lt_num(i, n);
  default: return 0;
  }
}

/* Compare two objects without calling metamethods. */
int LJ_FASTCALL lj_obj_equal(cTValue *o1, cTValue *o2)
{
  int64_t i1, i2;
  if (itype(o1) == itype(o2)) {
    if (tvispri(o1))
      return 1;
    if (tvisi64(o1))
      return i64V(o1) == i64V(o2);
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
  } else if ((!tvisnumber(o1) && !tvisi64(o1)) ||
	     (!tvisnumber(o2) && !tvisi64(o2))) {
    return 0;
  } else if (obj_integer(o1, &i1)) {
    return obj_integer(o2, &i2) ? i1 == i2 : obj_numeq_i64(numV(o2), i1);
  } else if (obj_integer(o2, &i2)) {
    return obj_numeq_i64(numV(o1), i2);
  }
  return numberVnum(o1) == numberVnum(o2);
}

int LJ_FASTCALL lj_obj_equaltv(uint64_t u1, uint64_t u2)
{
  TValue o1, o2;
  o1.u64 = u1;
  o2.u64 = u2;
  return lj_obj_equal(&o1, &o2);
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
  else if (tvisi64(o))
    return NULL;
  else if (tvisgcv(o))
    return gcV(o);
  else
    return NULL;
}

