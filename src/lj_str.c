/*
** String handling.
** Copyright (C) 2005-2026 Mike Pall. See Copyright Notice in luajit.h
*/

#define lj_str_c
#define LUA_CORE

#include "lj_obj.h"
#include "lj_gc.h"
#include "lj_err.h"
#include "lj_str.h"
#include "lj_char.h"
#include "lj_prng.h"

/* -- String helpers ------------------------------------------------------ */

/* Ordered compare of strings. Assumes string data is 4-byte aligned. */
int32_t LJ_FASTCALL lj_str_cmp(GCstr *a, GCstr *b)
{
  MSize i, n = a->len > b->len ? b->len : a->len;
  for (i = 0; i < n; i += 4) {
    /* Note: innocuous access up to end of string + 3. */
    uint32_t va = *(const uint32_t *)(strdata(a)+i);
    uint32_t vb = *(const uint32_t *)(strdata(b)+i);
    if (va != vb) {
#if LJ_LE
      va = lj_bswap(va); vb = lj_bswap(vb);
#endif
      i -= n;
      if ((int32_t)i >= -3) {
	va >>= 32+(i<<3); vb >>= 32+(i<<3);
	if (va == vb) break;
      }
      return va < vb ? -1 : 1;
    }
  }
  return (int32_t)(a->len - b->len);
}

/* Lua 5.4 orders strings with the active collate locale and handles embedded
** NUL bytes by comparing each C-string segment, matching upstream l_strcmp().
** Keep lj_str_cmp() byte-stable for bytecode/table internals and use this only
** for user-visible ordered comparisons in 5.4 compatibility mode.
*/
int32_t LJ_FASTCALL lj_str_cmp_locale(GCstr *a, GCstr *b)
{
  const char *s1 = strdata(a);
  const char *s2 = strdata(b);
  MSize rl1 = a->len, rl2 = b->len;
  for (;;) {
    int temp = strcoll(s1, s2);
    if (temp != 0) {
      return temp;
    } else {
      MSize zl1 = (MSize)strlen(s1);
      MSize zl2 = (MSize)strlen(s2);
      if (zl2 == rl2)
	return zl1 == rl1 ? 0 : 1;
      else if (zl1 == rl1)
	return -1;
      zl1++; zl2++;
      s1 += zl1; rl1 -= zl1; s2 += zl2; rl2 -= zl2;
    }
  }
}

/* Equality compare of strings. Handles non-interned Lua 5.4 long strings. */
int LJ_FASTCALL lj_str_equal(GCstr *a, GCstr *b)
{
  return a == b || (LJ_54 && a->len == b->len &&
		    memcmp(strdata(a), strdata(b), a->len) == 0);
}

/* Find fixed string p inside string s. */
const char *lj_str_find(const char *s, const char *p, MSize slen, MSize plen)
{
  if (plen <= slen) {
    if (plen == 0) {
      return s;
    } else {
      int c = *(const uint8_t *)p++;
      plen--; slen -= plen;
      while (slen) {
	const char *q = (const char *)memchr(s, c, slen);
	if (!q) break;
	if (memcmp(q+1, p, plen) == 0) return q;
	q++; slen -= (MSize)(q-s); s = q;
      }
    }
  }
  return NULL;
}

/* Check whether a string has a pattern matching character. */
int lj_str_haspattern(GCstr *s)
{
  const char *p = strdata(s), *q = p + s->len;
  while (p < q) {
    int c = *(const uint8_t *)p++;
    if (lj_char_ispunct(c) && strchr("^$*+?.([%-", c))
      return 1;  /* Found a pattern matching char. */
  }
  return 0;  /* No pattern matching chars found. */
}

/* -- String hashing ------------------------------------------------------ */

/* Keyed sparse ARX string hash. Constant time. */
static StrHash hash_sparse(uint64_t seed, const char *str, MSize len)
{
  /* Constants taken from lookup3 hash by Bob Jenkins. */
  StrHash a, b, h = len ^ (StrHash)seed;
  if (len >= 4) {  /* Caveat: unaligned access! */
    a = lj_getu32(str);
    h ^= lj_getu32(str+len-4);
    b = lj_getu32(str+(len>>1)-2);
    h ^= b; h -= lj_rol(b, 14);
    b += lj_getu32(str+(len>>2)-1);
  } else {
    a = *(const uint8_t *)str;
    h ^= *(const uint8_t *)(str+len-1);
    b = *(const uint8_t *)(str+(len>>1));
    h ^= b; h -= lj_rol(b, 14);
  }
  a ^= h; a -= lj_rol(h, 11);
  b ^= a; b -= lj_rol(a, 25);
  h ^= b; h -= lj_rol(b, 16);
  return h;
}

#if LUAJIT_SECURITY_STRHASH
/* Keyed dense ARX string hash. Linear time. */
static LJ_NOINLINE StrHash hash_dense(uint64_t seed, StrHash h,
				      const char *str, MSize len)
{
  StrHash b = lj_bswap(lj_rol(h ^ (StrHash)(seed >> 32), 4));
  if (len > 12) {
    StrHash a = (StrHash)seed;
    const char *pe = str+len-12, *p = pe, *q = str;
    do {
      a += lj_getu32(p);
      b += lj_getu32(p+4);
      h += lj_getu32(p+8);
      p = q; q += 12;
      h ^= b; h -= lj_rol(b, 14);
      a ^= h; a -= lj_rol(h, 11);
      b ^= a; b -= lj_rol(a, 25);
    } while (p < pe);
    h ^= b; h -= lj_rol(b, 16);
    a ^= h; a -= lj_rol(h, 4);
    b ^= a; b -= lj_rol(a, 14);
  }
  return b;
}
#endif

#if LJ_54 && LJ_TARGET_ARM64
static LJ_AINLINE uint8_t str_cat_byte(GCstr *s1, GCstr *s2, MSize pos)
{
  MSize len1 = s1->len;
  return pos < len1 ? (uint8_t)strdata(s1)[pos] :
		      (uint8_t)strdata(s2)[pos - len1];
}

static LJ_AINLINE uint32_t str_cat_getu32(GCstr *s1, GCstr *s2, MSize pos)
{
  MSize len1 = s1->len;
  if (pos + 4 <= len1)
    return lj_getu32(strdata(s1) + pos);
  if (pos >= len1)
    return lj_getu32(strdata(s2) + (pos - len1));
  {
    uint32_t b0 = str_cat_byte(s1, s2, pos);
    uint32_t b1 = str_cat_byte(s1, s2, pos + 1);
    uint32_t b2 = str_cat_byte(s1, s2, pos + 2);
    uint32_t b3 = str_cat_byte(s1, s2, pos + 3);
#if LJ_LE
    return b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
#else
    return (b0 << 24) | (b1 << 16) | (b2 << 8) | b3;
#endif
  }
}

static StrHash hash_sparse_cat(uint64_t seed, GCstr *s1, GCstr *s2, MSize len)
{
  StrHash a, b, h = len ^ (StrHash)seed;
  if (len >= 4) {
    a = str_cat_getu32(s1, s2, 0);
    h ^= str_cat_getu32(s1, s2, len-4);
    b = str_cat_getu32(s1, s2, (len>>1)-2);
    h ^= b; h -= lj_rol(b, 14);
    b += str_cat_getu32(s1, s2, (len>>2)-1);
  } else {
    a = str_cat_byte(s1, s2, 0);
    h ^= str_cat_byte(s1, s2, len-1);
    b = str_cat_byte(s1, s2, len>>1);
    h ^= b; h -= lj_rol(b, 14);
  }
  a ^= h; a -= lj_rol(h, 11);
  b ^= a; b -= lj_rol(a, 25);
  h ^= b; h -= lj_rol(b, 16);
  return h;
}

#if LUAJIT_SECURITY_STRHASH
static LJ_NOINLINE StrHash hash_dense_cat(uint64_t seed, StrHash h,
					  GCstr *s1, GCstr *s2, MSize len)
{
  StrHash b = lj_bswap(lj_rol(h ^ (StrHash)(seed >> 32), 4));
  if (len > 12) {
    StrHash a = (StrHash)seed;
    MSize pe = len-12, p = pe, q = 0;
    do {
      a += str_cat_getu32(s1, s2, p);
      b += str_cat_getu32(s1, s2, p+4);
      h += str_cat_getu32(s1, s2, p+8);
      p = q; q += 12;
      h ^= b; h -= lj_rol(b, 14);
      a ^= h; a -= lj_rol(h, 11);
      b ^= a; b -= lj_rol(a, 25);
    } while (p < pe);
    h ^= b; h -= lj_rol(b, 16);
    a ^= h; a -= lj_rol(h, 4);
    b ^= a; b -= lj_rol(a, 14);
  }
  return b;
}
#endif
#endif

/* -- String interning ---------------------------------------------------- */

#define LJ_STR_MAXCOLL		32

/* Resize the string interning hash table (grow and shrink). */
void lj_str_resize(lua_State *L, MSize newmask)
{
  global_State *g = G(L);
  GCRef *newtab, *oldtab = g->str.tab;
  MSize i;

  /* No resizing during GC traversal or if already too big. */
  if (g->gc.state == GCSsweepstring || newmask >= LJ_MAX_STRTAB-1)
    return;

  newtab = lj_mem_newvec(L, newmask+1, GCRef);
  memset(newtab, 0, (newmask+1)*sizeof(GCRef));

#if LUAJIT_SECURITY_STRHASH
  /* Check which chains need secondary hashes. */
  if (g->str.second) {
    int newsecond = 0;
    /* Compute primary chain lengths. */
    for (i = g->str.mask; i != ~(MSize)0; i--) {
      GCobj *o = (GCobj *)(gcrefu(oldtab[i]) & ~(uintptr_t)1);
      while (o) {
	GCstr *s = gco2str(o);
	MSize hash = s->hashalg ? hash_sparse(g->str.seed, strdata(s), s->len) :
				  s->hash;
	hash &= newmask;
	setgcrefp(newtab[hash], gcrefu(newtab[hash]) + 1);
	o = gcnext(o);
      }
    }
    /* Mark secondary chains. */
    for (i = newmask; i != ~(MSize)0; i--) {
      int secondary = gcrefu(newtab[i]) > LJ_STR_MAXCOLL;
      newsecond |= secondary;
      setgcrefp(newtab[i], secondary);
    }
    g->str.second = newsecond;
  }
#endif

  /* Reinsert all strings from the old table into the new table. */
  for (i = g->str.mask; i != ~(MSize)0; i--) {
    GCobj *o = (GCobj *)(gcrefu(oldtab[i]) & ~(uintptr_t)1);
    while (o) {
      GCobj *next = gcnext(o);
      GCstr *s = gco2str(o);
      MSize hash = s->hash;
#if LUAJIT_SECURITY_STRHASH
      uintptr_t u;
      if (LJ_LIKELY(!s->hashalg)) {  /* String hashed with primary hash. */
	hash &= newmask;
	u = gcrefu(newtab[hash]);
	if (LJ_UNLIKELY(u & 1)) {  /* Switch string to secondary hash. */
	  s->hash = hash = hash_dense(g->str.seed, s->hash, strdata(s), s->len);
	  s->hashalg = 1;
	  hash &= newmask;
	  u = gcrefu(newtab[hash]);
	}
      } else {  /* String hashed with secondary hash. */
	MSize shash = hash_sparse(g->str.seed, strdata(s), s->len);
	u = gcrefu(newtab[shash & newmask]);
	if (u & 1) {
	  hash &= newmask;
	  u = gcrefu(newtab[hash]);
	} else {  /* Revert string back to primary hash. */
	  s->hash = shash;
	  s->hashalg = 0;
	  hash = (shash & newmask);
	}
      }
      /* NOBARRIER: The string table is a GC root. */
      setgcrefp(o->gch.nextgc, (u & ~(uintptr_t)1));
      setgcrefp(newtab[hash], ((uintptr_t)o | (u & 1)));
#else
      hash &= newmask;
      /* NOBARRIER: The string table is a GC root. */
      setgcrefr(o->gch.nextgc, newtab[hash]);
      setgcref(newtab[hash], o);
#endif
      o = next;
    }
  }

  /* Free old table and replace with new table. */
  lj_str_freetab(g);
  g->str.tab = newtab;
  g->str.mask = newmask;
}

#if LUAJIT_SECURITY_STRHASH
/* Rehash and rechain all strings in a chain. */
static LJ_NOINLINE GCstr *lj_str_rehash_chain(lua_State *L, StrHash hashc,
					      const char *str, MSize len)
{
  global_State *g = G(L);
  int ow = g->gc.state == GCSsweepstring ? otherwhite(g) : 0;  /* Sweeping? */
  GCRef *strtab = g->str.tab;
  MSize strmask = g->str.mask;
  GCobj *o = gcref(strtab[hashc & strmask]);
  setgcrefp(strtab[hashc & strmask], (void *)((uintptr_t)1));
  g->str.second = 1;
  while (o) {
    uintptr_t u;
    GCobj *next = gcnext(o);
    GCstr *s = gco2str(o);
    StrHash hash;
    if (ow) {  /* Must sweep while rechaining. */
      if (((o->gch.marked ^ LJ_GC_WHITES) & ow)) {  /* String alive? */
	lj_assertG(!isdead(g, o) || (o->gch.marked & LJ_GC_FIXED),
		   "sweep of undead string");
	makewhite(g, o);
      } else {  /* Free dead string. */
	lj_assertG(isdead(g, o) || ow == LJ_GC_SFIXED,
		   "sweep of unlive string");
	lj_str_free(g, s);
	o = next;
	continue;
      }
    }
    hash = s->hash;
    if (!s->hashalg) {  /* Rehash with secondary hash. */
      hash = hash_dense(g->str.seed, hash, strdata(s), s->len);
      s->hash = hash;
      s->hashalg = 1;
    }
    /* Rechain. */
    hash &= strmask;
    u = gcrefu(strtab[hash]);
    setgcrefp(o->gch.nextgc, (u & ~(uintptr_t)1));
    setgcrefp(strtab[hash], ((uintptr_t)o | (u & 1)));
    o = next;
  }
  /* Try to insert the pending string again. */
  return lj_str_new(L, str, len);
}
#endif

/* Reseed String ID from PRNG after random interval < 2^bits. */
#if LUAJIT_SECURITY_STRID == 1
#define STRID_RESEED_INTERVAL	8
#elif LUAJIT_SECURITY_STRID == 2
#define STRID_RESEED_INTERVAL	4
#elif LUAJIT_SECURITY_STRID >= 3
#define STRID_RESEED_INTERVAL	0
#endif

/* Allocate a new string and add to string table. */
static GCstr *lj_str_alloc(lua_State *L, const char *str, MSize len,
			   StrHash hash, int hashalg,
			   int stable_sid, StrID sid)
{
  GCstr *s = lj_mem_newt(L, lj_str_size(len), GCstr);
  global_State *g = G(L);
  uintptr_t u;
  newwhite(g, s);
  s->gct = ~LJ_TSTR;
  s->len = len;
  s->hash = hash;
  if (stable_sid) {
    /* Lua 5.4 long strings are not interned. Give equal long-string contents
    ** the same table hash key, while keeping the actual GC object distinct.
    */
    s->sid = sid;
  } else {
#ifndef STRID_RESEED_INTERVAL
    s->sid = g->str.id++;
#elif STRID_RESEED_INTERVAL
    if (!g->str.idreseed--) {
      uint64_t r = lj_prng_u64(&g->prng);
      g->str.id = (StrID)r;
      g->str.idreseed = (uint8_t)(r >> (64 - STRID_RESEED_INTERVAL));
    }
    s->sid = g->str.id++;
#else
    s->sid = (StrID)lj_prng_u64(&g->prng);
#endif
  }
  s->reserved = 0;
  s->hashalg = (uint8_t)hashalg;
  /* Clear last 4 bytes of allocated memory. Implies zero-termination, too. */
  *(uint32_t *)(strdatawr(s)+(len & ~(MSize)3)) = 0;
  memcpy(strdatawr(s), str, len);
  /* Add to string hash table. */
  hash &= g->str.mask;
  u = gcrefu(g->str.tab[hash]);
  setgcrefp(s->nextgc, (u & ~(uintptr_t)1));
  /* NOBARRIER: The string table is a GC root. */
  setgcrefp(g->str.tab[hash], ((uintptr_t)s | (u & 1)));
  if (g->str.num++ > g->str.mask)  /* Allow a 100% load factor. */
    lj_str_resize(L, (g->str.mask<<1)+1);  /* Grow string table. */
  return s;  /* Return newly interned string. */
}

#if LJ_54 && LJ_TARGET_ARM64
static GCstr *lj_str_alloc_cat2(lua_State *L, GCstr *s1, GCstr *s2, MSize len,
				StrHash hash, int hashalg)
{
  GCstr *s = lj_mem_newt(L, lj_str_size(len), GCstr);
  global_State *g = G(L);
  uintptr_t u;
  char *data;
  newwhite(g, s);
  s->gct = ~LJ_TSTR;
  s->len = len;
  s->hash = hash;
  s->sid = (StrID)hash;
  s->reserved = 0;
  s->hashalg = (uint8_t)hashalg;
  data = strdatawr(s);
  *(uint32_t *)(data+(len & ~(MSize)3)) = 0;
  memcpy(data, strdata(s1), s1->len);
  memcpy(data + s1->len, strdata(s2), s2->len);
  hash &= g->str.mask;
  u = gcrefu(g->str.tab[hash]);
  setgcrefp(s->nextgc, (u & ~(uintptr_t)1));
  setgcrefp(g->str.tab[hash], ((uintptr_t)s | (u & 1)));
  if (g->str.num++ > g->str.mask)
    lj_str_resize(L, (g->str.mask<<1)+1);
  return s;
}
#endif

/* Create a string and return string object. Parser constants may force
** interning even for Lua 5.4 long strings; runtime-created long strings must
** stay distinct objects.
*/
static GCstr *lj_str_newx(lua_State *L, const char *str, size_t lenx,
			  int intern_long)
{
  global_State *g = G(L);
  if (lenx-1 < LJ_MAX_STR-1) {
    MSize len = (MSize)lenx;
    StrHash hash = hash_sparse(g->str.seed, str, len);
    StrID sid = (StrID)hash;
    int nointern = LJ_54 && len > LJ_STR_MAXSHORT && !intern_long;
    MSize coll = 0;
    int hashalg = 0;
    /* Check if the string has already been interned. */
    GCobj *o = gcref(g->str.tab[hash & g->str.mask]);
#if LUAJIT_SECURITY_STRHASH
    if (LJ_UNLIKELY((uintptr_t)o & 1)) {  /* Secondary hash for this chain? */
      hashalg = 1;
      hash = hash_dense(g->str.seed, hash, str, len);
      o = (GCobj *)(gcrefu(g->str.tab[hash & g->str.mask]) & ~(uintptr_t)1);
    }
#endif
    while (o != NULL) {
      GCstr *sx = gco2str(o);
      if (sx->hash == hash && sx->len == len) {
	if (memcmp(str, strdata(sx), len) == 0) {
	  if (!nointern) {
	    if (isdead(g, o)) flipwhite(o);  /* Resurrect if dead. */
	    return sx;  /* Return existing short string. */
	  }
	  /* Equal long strings deliberately remain separate GC objects in Lua 5.4. */
	}
	coll++;
      }
      coll++;
      o = gcnext(o);
    }
#if LUAJIT_SECURITY_STRHASH
    /* Rehash chain if there are too many collisions. */
    if (LJ_UNLIKELY(coll > LJ_STR_MAXCOLL) && !hashalg) {
      return lj_str_rehash_chain(L, hash, str, len);
    }
#endif
    /* Otherwise allocate a new string. */
    return lj_str_alloc(L, str, len, hash, hashalg,
#if LJ_TARGET_ARM64
			LJ_54 && len > LJ_STR_MAXSHORT ? 1 : nointern,
#else
			nointern,
#endif
			sid);
  } else {
    if (lenx)
      lj_err_msg(L, LJ_ERR_STROV);
    return &g->strempty;
  }
}

/* Intern short strings, but create distinct runtime long strings in Lua 5.4. */
GCstr *lj_str_new(lua_State *L, const char *str, size_t lenx)
{
  return lj_str_newx(L, str, lenx, 0);
}

/* Intern parser/source constants, including Lua 5.4 long string literals. */
GCstr *lj_str_new_intern(lua_State *L, const char *str, size_t lenx)
{
  return lj_str_newx(L, str, lenx, 1);
}

#if LJ_54 && LJ_TARGET_ARM64
GCstr *lj_str_new_noscan(lua_State *L, const char *str, size_t lenx)
{
  global_State *g = G(L);
  if (lenx-1 < LJ_MAX_STR-1) {
    MSize len = (MSize)lenx;
    StrHash hash;
    int hashalg = 0;
    if (len <= LJ_STR_MAXSHORT)
      return lj_str_new(L, str, len);
    hash = hash_sparse(g->str.seed, str, len);
#if LUAJIT_SECURITY_STRHASH
    if (LJ_UNLIKELY((uintptr_t)gcref(g->str.tab[hash & g->str.mask]) & 1)) {
      hashalg = 1;
      hash = hash_dense(g->str.seed, hash, str, len);
    }
#endif
    return lj_str_alloc(L, str, len, hash, hashalg, 1, (StrID)hash);
  } else {
    if (lenx)
      lj_err_msg(L, LJ_ERR_STROV);
    return &g->strempty;
  }
}

GCstr *lj_str_new_cat2(lua_State *L, GCstr *s1, GCstr *s2)
{
  global_State *g = G(L);
  size_t lenx = (size_t)s1->len + (size_t)s2->len;
  if (lenx-1 < LJ_MAX_STR-1) {
    MSize len = (MSize)lenx;
    StrHash hash = hash_sparse_cat(g->str.seed, s1, s2, len);
    int hashalg = 0;
#if LUAJIT_SECURITY_STRHASH
    GCobj *o = gcref(g->str.tab[hash & g->str.mask]);
    if (LJ_UNLIKELY((uintptr_t)o & 1)) {
      hashalg = 1;
      hash = hash_dense_cat(g->str.seed, hash, s1, s2, len);
    }
#endif
    return lj_str_alloc_cat2(L, s1, s2, len, hash, hashalg);
  } else {
    lj_err_msg(L, LJ_ERR_STROV);
    return &g->strempty;  /* unreachable */
  }
}
#endif

void LJ_FASTCALL lj_str_free(global_State *g, GCstr *s)
{
  g->str.num--;
  lj_mem_free(g, s, lj_str_size(s->len));
}

void LJ_FASTCALL lj_str_init(lua_State *L)
{
  global_State *g = G(L);
  g->str.seed = lj_prng_u64(&g->prng);
  lj_str_resize(L, LJ_MIN_STRTAB-1);
}
