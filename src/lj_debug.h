/*
** Debugging and introspection.
** Copyright (C) 2005-2026 Mike Pall. See Copyright Notice in luajit.h
*/

#ifndef _LJ_DEBUG_H
#define _LJ_DEBUG_H

#include "lj_obj.h"

typedef struct lj_Debug {
  /* Common fields. Must be in the same order as in lua.h. */
  int event;
  const char *name;
  const char *namewhat;
  const char *what;
  const char *source;
#if LJ_54
  size_t srclen;
  int currentline;
  int linedefined;
  int lastlinedefined;
  uint8_t nups;
  uint8_t nparams;
  char isvararg;
  char istailcall;
  unsigned short ftransfer;
  unsigned short ntransfer;
  char short_src[LUA_IDSIZE];
  struct CallInfo *i_ci;
#else
  int currentline;
  int nups;
  int linedefined;
  int lastlinedefined;
  char short_src[LUA_IDSIZE];
  int nparams;
  int isvararg;
  int istailcall;
  unsigned short ftransfer;
  unsigned short ntransfer;
  int i_ci;
#endif
} lj_Debug;

#if LJ_54
#define LJ_DEBUG_CI_ENCODE(ci)	((struct CallInfo *)(intptr_t)(ci))
#define LJ_DEBUG_CI_VALUE(ci)	((intptr_t)(ci))
#else
#define LJ_DEBUG_CI_ENCODE(ci)	((int)(ci))
#define LJ_DEBUG_CI_VALUE(ci)	((intptr_t)(ci))
#endif

LJ_FUNC cTValue *lj_debug_frame(lua_State *L, int level, int *size);
LJ_FUNC BCLine LJ_FASTCALL lj_debug_line(GCproto *pt, BCPos pc);
LJ_FUNC const char *lj_debug_uvname(GCproto *pt, uint32_t idx);
LJ_FUNC const char *lj_debug_uvnamev(cTValue *o, uint32_t idx, TValue **tvp,
				     GCobj **op);
LJ_FUNC int lj_debug_hasenvuv(GCfunc *fn);
LJ_FUNC const char *lj_debug_slotname(GCproto *pt, const BCIns *pc,
				      BCReg slot, const char **name);
LJ_FUNC BCPos lj_debug_framepc(lua_State *L, GCfunc *fn, cTValue *nextframe);
LJ_FUNC const char *lj_debug_funcname(lua_State *L, cTValue *frame,
				      const char **name);
#if LJ_54
LJ_FUNC const char *lj_debug_callname54(lua_State *L, const char *fallback,
					const char *prefix);
#endif
LJ_FUNC void lj_debug_shortname(char *out, GCstr *str, BCLine line);
LJ_FUNC void lj_debug_addloc(lua_State *L, const char *msg,
			     cTValue *frame, cTValue *nextframe);
LJ_FUNC void lj_debug_pushloc(lua_State *L, GCproto *pt, BCPos pc);
LJ_FUNC int lj_debug_getinfo(lua_State *L, const char *what, lj_Debug *ar,
			     int ext);
#if LJ_HASPROFILE
LJ_FUNC void lj_debug_dumpstack(lua_State *L, SBuf *sb, const char *fmt,
				int depth);
#endif

/* Fixed internal variable names. */
#define VARNAMEDEF(_) \
  _(FOR_IDX, "(for index)") \
  _(FOR_STOP, "(for limit)") \
  _(FOR_STEP, "(for step)") \
  _(FOR_GEN, "(for generator)") \
  _(FOR_STATE, "(for state)") \
  _(FOR_CTL, "(for control)")

enum {
  VARNAME_END,
#define VARNAMEENUM(name, str)	VARNAME_##name,
  VARNAMEDEF(VARNAMEENUM)
#undef VARNAMEENUM
  VARNAME__MAX
};

#endif
