/*
** Pseudo-random number generation.
** Copyright (C) 2005-2026 Mike Pall. See Copyright Notice in luajit.h
*/

#ifndef _LJ_PRNG_H
#define _LJ_PRNG_H

#include "lj_def.h"

LJ_FUNC int LJ_FASTCALL lj_prng_seed_secure(PRNGState *rs);
LJ_FUNC uint64_t LJ_FASTCALL lj_prng_u64(PRNGState *rs);
LJ_FUNC uint64_t LJ_FASTCALL lj_prng_u64d(PRNGState *rs);
LJ_FUNC lua_Number LJ_FASTCALL lj_prng_num_random54(PRNGState *rs);
LJ_FUNC int32_t LJ_FASTCALL lj_prng_i32_random54(PRNGState *rs);
LJ_FUNC int32_t lj_prng_int_random54(PRNGState *rs, int32_t low, int32_t up);

/* This is just the precomputed result of lib_math.c:random_seed(rs, 0.0). */
static LJ_AINLINE void lj_prng_seed_fixed(PRNGState *rs)
{
  rs->u[0] = U64x(a0d27757,0a345b8c);
  rs->u[1] = U64x(764a296c,5d4aa64f);
  rs->u[2] = U64x(51220704,070adeaa);
  rs->u[3] = U64x(2a2717b5,a7b7b927);
}

#endif
