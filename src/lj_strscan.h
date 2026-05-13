/*
** String scanning.
** Copyright (C) 2005-2026 Mike Pall. See Copyright Notice in luajit.h
*/

#ifndef _LJ_STRSCAN_H
#define _LJ_STRSCAN_H

#include "lj_obj.h"

/* Options for accepted/returned formats. */
#define STRSCAN_OPT_TOINT	0x01  /* Convert to int32_t, if possible. */
#define STRSCAN_OPT_TONUM	0x02  /* Always convert to double. */
#define STRSCAN_OPT_IMAG	0x04
#define STRSCAN_OPT_LL		0x08
#define STRSCAN_OPT_C		0x10

/* Returned format. */
typedef enum {
  STRSCAN_ERROR,
  STRSCAN_NUM, STRSCAN_IMAG,
  STRSCAN_INT, STRSCAN_U32, STRSCAN_I64, STRSCAN_U64,
} StrScanFmt;

LJ_FUNC StrScanFmt lj_strscan_scan(const uint8_t *p, MSize len, TValue *o,
				   uint32_t opt);
#if LJ_54
LJ_FUNC int lj_strscan_rejectnum54(const char *p, MSize len);
LJ_FUNC int lj_strscan_tobaseint54(GCstr *str, int32_t base, int32_t *ip);
#endif
LJ_FUNC int LJ_FASTCALL lj_strscan_num(GCstr *str, TValue *o);
LJ_FUNC int LJ_FASTCALL lj_strscan_numtype54(GCstr *str);
LJ_FUNC int LJ_FASTCALL lj_strscan_numtype54s(GCstr *str);
LJ_FUNC int32_t LJ_FASTCALL lj_strscan_toint54(GCstr *str);
LJ_FUNC lua_Number LJ_FASTCALL lj_strscan_tonum54s(GCstr *str);
LJ_FUNC int LJ_FASTCALL lj_strscan_toi64ok54(GCstr *str);
LJ_FUNC int64_t LJ_FASTCALL lj_strscan_toi6454(GCstr *str);
LJ_FUNC int LJ_FASTCALL lj_strscan_tocheckintok54(GCstr *str);
LJ_FUNC int32_t LJ_FASTCALL lj_strscan_tocheckint54(GCstr *str);
LJ_FUNC int lj_strscan_tobaseintok54(GCstr *str, int32_t base);
LJ_FUNC int32_t lj_strscan_tobaseintvalue54(GCstr *str, int32_t base);
#if LJ_DUALNUM
LJ_FUNC int LJ_FASTCALL lj_strscan_number(GCstr *str, TValue *o);
#else
#define lj_strscan_number(s, o)		lj_strscan_num((s), (o))
#endif

/* Check for number or convert string to number/int in-place (!). */
static LJ_AINLINE int lj_strscan_numberobj(TValue *o)
{
  return tvisnumber(o) || tvisi64(o) ||
	 (tvisstr(o) && lj_strscan_number(strV(o), o));
}

#endif
