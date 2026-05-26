/*
** I/O library.
** Copyright (C) 2005-2026 Mike Pall. See Copyright Notice in luajit.h
**
** Major portions taken verbatim or adapted from the Lua interpreter.
** Copyright (C) 1994-2011 Lua.org, PUC-Rio. See Copyright Notice in lua.h
*/

#include <errno.h>
#include <limits.h>
#include <stdio.h>

#define lib_io_c
#define LUA_LIB

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include "lj_obj.h"
#include "lj_gc.h"
#include "lj_err.h"
#include "lj_buf.h"
#include "lj_str.h"
#include "lj_char.h"
#include "lj_meta.h"
#include "lj_debug.h"
#include "lj_frame.h"
#include "lj_state.h"
#include "lj_strfmt.h"
#include "lj_ff.h"
#include "lj_lib.h"
#include "lj_strscan.h"

/* Userdata payload for I/O file. */
typedef struct IOFileUD {
  FILE *fp;		/* File handle. */
  uint32_t type;	/* File type. */
} IOFileUD;

#define IOFILE_TYPE_FILE	0	/* Regular file. */
#define IOFILE_TYPE_PIPE	1	/* Pipe. */
#define IOFILE_TYPE_STDF	2	/* Standard file handle. */
#define IOFILE_TYPE_MASK	3

#define IOFILE_FLAG_CLOSE	4	/* Close after io.lines() iterator. */

#define IOSTDF_UD(L, id)	(&gcref(G(L)->gcroot[(id)])->ud)
#define IOSTDF_IOF(L, id)	((IOFileUD *)uddata(IOSTDF_UD(L, (id))))

/* -- Open/close helpers -------------------------------------------------- */

static IOFileUD *io_tofilep(lua_State *L)
{
  if (!(L->base < L->top && tvisudata(L->base) &&
	udataV(L->base)->udtype == UDTYPE_IO_FILE))
    lj_err_argtype(L, 1, "FILE*");
  return (IOFileUD *)uddata(udataV(L->base));
}

#if !LJ_54
static IOFileUD *io_tofile(lua_State *L)
{
  IOFileUD *iof = io_tofilep(L);
  if (iof->fp == NULL)
    lj_err_caller(L, LJ_ERR_IOCLFL);
  return iof;
}
#endif

static IOFileUD *io_stdfile(lua_State *L, ptrdiff_t id)
{
  IOFileUD *iof = IOSTDF_IOF(L, id);
  if (iof->fp == NULL) {
#if LJ_54
    lj_err_callermsg(L, id == GCROOT_IO_INPUT ?
		     "default input file is closed" :
		     "default output file is closed");
#endif
    lj_err_caller(L, LJ_ERR_IOSTDCL);
  }
  return iof;
}

static IOFileUD *io_file_new(lua_State *L)
{
  IOFileUD *iof = (IOFileUD *)lua_newuserdata(L, sizeof(IOFileUD));
  GCudata *ud = udataV(L->top-1);
  ud->udtype = UDTYPE_IO_FILE;
  /* NOBARRIER: The GCudata is new (marked white). */
  setgcrefr(ud->metatable, curr_func(L)->c.env);
  iof->fp = NULL;
  iof->type = IOFILE_TYPE_FILE;
  return iof;
}

static IOFileUD *io_file_open(lua_State *L, const char *mode)
{
  const char *fname = strdata(lj_lib_checkstr(L, 1));
  IOFileUD *iof = io_file_new(L);
  iof->fp = fopen(fname, mode);
  if (iof->fp == NULL)
#if LJ_54
    luaL_error(L, "cannot open file '%s' (%s)", fname, strerror(errno));
#else
    luaL_argerror(L, 1, lj_strfmt_pushf(L, "%s: %s", fname, strerror(errno)));
#endif
  return iof;
}

static int io_file_close(lua_State *L, IOFileUD *iof)
{
  int ok;
  if ((iof->type & IOFILE_TYPE_MASK) == IOFILE_TYPE_FILE) {
    ok = (fclose(iof->fp) == 0);
  } else if ((iof->type & IOFILE_TYPE_MASK) == IOFILE_TYPE_PIPE) {
    int stat = -1;
#if LJ_TARGET_POSIX
    stat = pclose(iof->fp);
#elif LJ_TARGET_WINDOWS && !LJ_TARGET_XBOXONE && !LJ_TARGET_UWP
    stat = _pclose(iof->fp);
#endif
#if LJ_52
    iof->fp = NULL;
    return luaL_execresult(L, stat);
#else
    ok = (stat != -1);
#endif
  } else {
    lj_assertL((iof->type & IOFILE_TYPE_MASK) == IOFILE_TYPE_STDF,
	       "close of unknown FILE* type");
    setnilV(L->top++);
    lua_pushliteral(L, "cannot close standard file");
    return 2;
  }
  iof->fp = NULL;
  return luaL_fileresult(L, ok, NULL);
}

#if LJ_54
static void io_argerror54(lua_State *L, const char *fname, int narg,
			  const char *msg);
static const char *io_typename54(lua_State *L, int cidx);

static int io_checkmode_lua54(const char *mode)
{
  /* Lua 5.4 validates open modes itself: first r/w/a, optional '+', then any
  ** binary markers. This intentionally rejects legacy C-library extensions.
  */
  if (*mode == '\0' || strchr("rwa", *mode++) == NULL)
    return 0;
  if (*mode == '+')
    mode++;
  while (*mode == 'b')
    mode++;
  return *mode == '\0';
}

static const char *io_checkmode(lua_State *L, int arg, GCstr *s,
				const char *defmode, const char *fname)
{
  const char *mode = s ? strdata(s) : defmode;
  if (!io_checkmode_lua54(mode))
    io_argerror54(L, fname, arg, "invalid mode");
  return mode;
}

static void io_methodargerror54(lua_State *L, const char *fname, int narg,
				const char *msg)
{
  /* Lua reports file methods without the hidden self slot in public argument
  ** numbers. Keep audited method diagnostics local so unrelated legacy I/O
  ** messages stay unchanged until their own boundaries are checked.
  */
  fname = lj_debug_callname54(L, fname, "io");
  lj_err_callermsg(L, lj_strfmt_pushf(L, "bad argument #%d to '%s' (%s)",
				      narg, fname, msg));
}

static int io_method_direct_pcall54(lua_State *L)
{
  cTValue *frame = L->base-1;
  if (frame > tvref(L->stack)+LJ_FR2) {
    if (frame_isvarg(frame))
      frame = frame_prevd(frame);
    if (frame > tvref(L->stack)+LJ_FR2)
      return frame_ispcall(frame);
  }
  return 0;
}

static const char *io_method_callinfo54(lua_State *L, const char *fallback,
					int *hiddenself)
{
  const char *name = "?";
  const char *kind = lj_debug_funcname(L, L->base-1, &name);
  *hiddenself = 0;
  if (kind && name) {
    if (strcmp(kind, "method") == 0)
      *hiddenself = 1;
    if (io_method_direct_pcall54(L) && strcmp(kind, "function") == 0)
      return "?";
    return name;
  }
  return io_method_direct_pcall54(L) ? "?" : fallback;
}

static void io_methodargerror_at54(lua_State *L, const char *fname, int cidx,
				   const char *msg)
{
  int hiddenself;
  fname = io_method_callinfo54(L, fname, &hiddenself);
  lj_err_callermsg(L, lj_strfmt_pushf(L, "bad argument #%d to '%s' (%s)",
				      cidx - hiddenself, fname, msg));
}

static int io_method_checkopt54(lua_State *L, const char *fname, int cidx,
				int def, const char *lst)
{
  TValue *o = L->base + cidx-1;
  GCstr *s = NULL;
  if (o < L->top && !tvisnil(o)) {
    if (tvisstr(o)) {
      s = strV(o);
    } else if (tvisnumber(o) || tvisi64(o)) {
      s = lj_strfmt_number(L, o);
      setstrV(L, o, s);
    } else {
      io_methodargerror_at54(L, fname, cidx,
	lj_strfmt_pushf(L, "string expected, got %s",
			io_typename54(L, cidx)));
    }
  } else if (def < 0) {
    io_methodargerror_at54(L, fname, cidx,
      lj_strfmt_pushf(L, "string expected, got %s", io_typename54(L, cidx)));
  }
  if (s) {
    const char *opt = strdata(s);
    MSize len = s->len;
    int i;
    for (i = 0; *(const uint8_t *)lst; i++) {
      if (*(const uint8_t *)lst == len && memcmp(opt, lst+1, len) == 0)
	return i;
      lst += 1+*(const uint8_t *)lst;
    }
    io_methodargerror_at54(L, fname, cidx,
			   lj_strfmt_pushf(L, "invalid option '%s'", opt));
  }
  return def;
}

static void io_methodselfargerror54(lua_State *L, const char *fname,
				    const char *msg)
{
  const char *name = "?";
  const char *kind = lj_debug_funcname(L, L->base-1, &name);
  if (kind && name) {
    fname = name;
  } else {
    fname = io_method_direct_pcall54(L) ? "?" :
	    lj_debug_callname54(L, fname, "io");
  }
  lj_err_callermsg(L, lj_strfmt_pushf(L, "bad argument #%d to '%s' (%s)",
				      1, fname, msg));
}

static const char *io_typename54(lua_State *L, int cidx)
{
  TValue *o = L->base + cidx-1;
  if (o < L->top) {
    MSize tlen;
    const char *tname = lj_meta_objtypename(L, o, &tlen);
    UNUSED(tlen);
    return tname;
  }
  return lj_obj_typename[0];
}

static void io_argerror54(lua_State *L, const char *fname, int narg,
			  const char *msg)
{
  fname = lj_debug_callname54(L, fname, "io");
  lj_err_callermsg(L, lj_strfmt_pushf(L, "bad argument #%d to '%s' (%s)",
				      narg, fname, msg));
}

static void io_argtype54(lua_State *L, const char *fname, int narg,
			 const char *xname)
{
  io_argerror54(L, fname, narg,
    lj_strfmt_pushf(L, "%s expected, got %s", xname,
		    io_typename54(L, narg)));
}

static cTValue *io_checkany54(lua_State *L, const char *fname, int narg)
{
  cTValue *o = L->base + narg-1;
  if (o >= L->top)
    io_argerror54(L, fname, narg, "value expected");
  return o;
}

static GCstr *io_checkstr_named54(lua_State *L, int narg, const char *fname)
{
  TValue *o = L->base + narg-1;
  if (o < L->top) {
    if (tvisstr(o)) {
      return strV(o);
    } else if (tvisnumber(o) || tvisi64(o)) {
      GCstr *s = lj_strfmt_number(L, o);
      setstrV(L, o, s);
      return s;
    }
  }
  io_argtype54(L, fname, narg, "string");
  return NULL;  /* Unreachable. */
}

static GCstr *io_optstr_named54(lua_State *L, int narg, const char *fname)
{
  TValue *o = L->base + narg-1;
  if (o >= L->top || tvisnil(o))
    return NULL;
  return io_checkstr_named54(L, narg, fname);
}

static IOFileUD *io_tofile_named54(lua_State *L, const char *fname)
{
  if (!(L->base < L->top && tvisudata(L->base) &&
	udataV(L->base)->udtype == UDTYPE_IO_FILE)) {
    io_argtype54(L, fname, 1, LUA_FILEHANDLE);
  }
  return (IOFileUD *)uddata(udataV(L->base));
}

static IOFileUD *io_file_named54(lua_State *L, const char *fname)
{
  IOFileUD *iof = io_tofile_named54(L, fname);
  if (iof->fp == NULL)
    lj_err_caller(L, LJ_ERR_IOCLFL);
  return iof;
}

static IOFileUD *io_method_tofile_named54(lua_State *L, const char *fname)
{
  if (!(L->base < L->top && tvisudata(L->base) &&
	udataV(L->base)->udtype == UDTYPE_IO_FILE)) {
    /* Dot-called file methods have no hidden self. Report the public method
    ** name and Lua 5.4's "FILE* expected" detail instead of the legacy '?'.
    */
    io_methodselfargerror54(L, fname,
      lj_strfmt_pushf(L, "%s expected, got %s", LUA_FILEHANDLE,
		      io_typename54(L, 1)));
  }
  return (IOFileUD *)uddata(udataV(L->base));
}

static IOFileUD *io_method_file_named54(lua_State *L, const char *fname)
{
  IOFileUD *iof = io_method_tofile_named54(L, fname);
  if (iof->fp == NULL)
    lj_err_caller(L, LJ_ERR_IOCLFL);
  return iof;
}

static void io_seekargtype54(lua_State *L, const char *xname)
{
  io_methodargerror_at54(L, "seek", 3,
    lj_strfmt_pushf(L, "%s expected, got %s", xname, io_typename54(L, 3)));
}

static int64_t io_checkseekofs54(lua_State *L)
{
  TValue tmp;
  cTValue *o = L->base+2;
  int64_t k;
  if (o >= L->top || tvisnil(o))
    return 0;
  if (tvisstr(o)) {
    if (!lj_strscan_number54(L, strV(o), &tmp))
      io_seekargtype54(L, "number");
    o = &tmp;
  }
  if (tvisint(o)) {
    k = (int64_t)intV(o);
  } else if (tvisi64(o)) {
    k = (int64_t)i64V(o);
  } else if (tvisnum(o)) {
    lua_Number n = numV(o);
    /* file:seek takes a lua_Integer offset in Lua 5.4.  Do not truncate
    ** fractions or decimal strings such as "1.5" before passing them to C.
    */
    if (!(n >= (lua_Number)INT64_MIN && n < -((lua_Number)INT64_MIN)))
      io_methodargerror_at54(L, "seek", 3,
			     "number has no integer representation");
    k = lj_num2i64(n);
    if ((lua_Number)k != n)
      io_methodargerror_at54(L, "seek", 3,
			     "number has no integer representation");
  } else {
    io_seekargtype54(L, "number");
    k = 0;  /* Unreachable. */
  }
#if defined(__MINGW32__) || (!LJ_TARGET_POSIX && \
    !(defined(_MSC_VER) && _MSC_VER >= 1400))
  if (k < (int64_t)LONG_MIN || k > (int64_t)LONG_MAX)
    io_methodargerror_at54(L, "seek", 3,
			   "not an integer in proper range");
#endif
  return k;
}

static void io_setvbufargtype54(lua_State *L, const char *xname)
{
  io_methodargerror_at54(L, "setvbuf", 3,
    lj_strfmt_pushf(L, "%s expected, got %s", xname, io_typename54(L, 3)));
}

static size_t io_checksetvbufsize54(lua_State *L)
{
  TValue tmp;
  cTValue *o = L->base+2;
  int64_t k;
  if (o >= L->top || tvisnil(o))
    return LUAL_BUFFERSIZE;
  if (tvisstr(o)) {
    if (!lj_strscan_number54(L, strV(o), &tmp))
      io_setvbufargtype54(L, "number");
    o = &tmp;
  }
  if (tvisint(o)) {
    k = (int64_t)intV(o);
  } else if (tvisi64(o)) {
    k = (int64_t)i64V(o);
  } else if (tvisnum(o)) {
    lua_Number n = numV(o);
    /* setvbuf's size is also a Lua 5.4 integer parameter. Reject fractions
    ** before calling C so the public result is an argument error, not a
    ** platform-dependent setvbuf failure.
    */
    if (!(n >= (lua_Number)INT64_MIN && n < -((lua_Number)INT64_MIN)))
      io_methodargerror_at54(L, "setvbuf", 3,
			     "number has no integer representation");
    k = lj_num2i64(n);
    if ((lua_Number)k != n)
      io_methodargerror_at54(L, "setvbuf", 3,
			     "number has no integer representation");
  } else {
    io_setvbufargtype54(L, "number");
    k = 0;  /* Unreachable. */
  }
  return (size_t)k;
}

static MSize io_checkreadlen54(lua_State *L, int cidx, int narg)
{
  cTValue *o = L->base + cidx-1;
  int64_t k;
  lua_Number n;
  if (tvisint(o))
    k = (int64_t)intV(o);
  else if (tvisi64(o))
    k = (int64_t)i64V(o);
  else {
    n = numV(o);
    /* Numeric read lengths are lua_Integer values in Lua 5.4.  Reject
    ** fractions before they are truncated into a shorter read.
    */
    if (!(n >= (lua_Number)INT64_MIN && n < -((lua_Number)INT64_MIN)))
      io_methodargerror54(L, "read", narg,
			  "number has no integer representation");
    k = lj_num2i64(n);
    if ((lua_Number)k != n)
      io_methodargerror54(L, "read", narg,
			  "number has no integer representation");
  }
  if (k < 0 || (uint64_t)k > (uint64_t)~(MSize)0)
    lj_err_mem(L);
  return (MSize)k;
}
#endif

/* -- Read/write helpers -------------------------------------------------- */

#if LJ_54
#define IO_MAXLENNUM	200

typedef struct IOReadNum {
  FILE *fp;
  int c;
  int n;
  char buf[IO_MAXLENNUM+1];
} IOReadNum;

static int io_readnum_nextc(IOReadNum *rn)
{
  if (rn->n >= IO_MAXLENNUM) {
    rn->buf[0] = '\0';
    return 0;
  }
  rn->buf[rn->n++] = (char)rn->c;
  rn->c = getc(rn->fp);
  return 1;
}

static int io_readnum_test2(IOReadNum *rn, const char *set)
{
  if (rn->c == set[0] || rn->c == set[1])
    return io_readnum_nextc(rn);
  return 0;
}

static int io_readnum_digits(IOReadNum *rn, int hex)
{
  int count = 0;
  while ((hex ? lj_char_isxdigit(rn->c) : lj_char_isdigit(rn->c)) &&
	 io_readnum_nextc(rn))
    count++;
  return count;
}

static int io_file_readnum(lua_State *L, FILE *fp)
{
  IOReadNum rn;
  int count = 0;
  int hex = 0;
  rn.fp = fp;
  rn.n = 0;
  do { rn.c = getc(fp); } while (lj_char_isspace(rn.c));
  io_readnum_test2(&rn, "-+");
  if (io_readnum_test2(&rn, "00")) {
    if (io_readnum_test2(&rn, "xX"))
      hex = 1;
    else
      count = 1;
  }
  count += io_readnum_digits(&rn, hex);
  if (io_readnum_test2(&rn, ".."))
    count += io_readnum_digits(&rn, hex);
  if (count > 0 && io_readnum_test2(&rn, hex ? "pP" : "eE")) {
    io_readnum_test2(&rn, "-+");
    io_readnum_digits(&rn, 0);
  }
  ungetc(rn.c, fp);
  rn.buf[rn.n] = '\0';
  /* Lua 5.4 reads the numeric prefix, then lets lua_stringtonumber() apply the
  ** same integer/float and hexadecimal rules as the rest of the language.
  */
  if (lua_stringtonumber(L, rn.buf))
    return 1;
  setnilV(L->top++);
  return 0;
}
#else
static int io_file_readnum(lua_State *L, FILE *fp)
{
  lua_Number d;
  if (fscanf(fp, LUA_NUMBER_SCAN, &d) == 1) {
    if (LJ_DUALNUM) {
      int64_t i64;
      int32_t i;
      if (lj_num2int_check(d, i64, i) && !tvismzero((cTValue *)&d)) {
	setintV(L->top++, i);
	return 1;
      }
    }
    setnumV(L->top++, d);
    return 1;
  } else {
    setnilV(L->top++);
    return 0;
  }
}
#endif

static int io_file_readline(lua_State *L, FILE *fp, MSize chop)
{
  MSize m = LUAL_BUFFERSIZE, n = 0, ok = 0;
  char *buf;
  for (;;) {
    buf = lj_buf_tmp(L, m);
    if (fgets(buf+n, m-n, fp) == NULL) break;
    n += (MSize)strlen(buf+n);
    ok |= n;
    if (n && buf[n-1] == '\n') { n -= chop; break; }
    if (n >= m - 64) m += m;
  }
  setstrV(L, L->top++, lj_str_new(L, buf, (size_t)n));
  lj_gc_check(L);
  return (int)ok;
}

static void io_file_readall(lua_State *L, FILE *fp)
{
  MSize m, n;
  for (m = LUAL_BUFFERSIZE, n = 0; ; m += m) {
    char *buf = lj_buf_tmp(L, m);
    n += (MSize)fread(buf+n, 1, m-n, fp);
    if (n != m) {
      setstrV(L, L->top++, lj_str_new(L, buf, (size_t)n));
      lj_gc_check(L);
      return;
    }
  }
}

static int io_file_readlen(lua_State *L, FILE *fp, MSize m)
{
  if (m) {
    char *buf = lj_buf_tmp(L, m);
    MSize n = (MSize)fread(buf, 1, m, fp);
    setstrV(L, L->top++, lj_str_new(L, buf, (size_t)n));
    lj_gc_check(L);
    return n > 0;
  } else {
    int c = getc(fp);
    ungetc(c, fp);
    setstrV(L, L->top++, &G(L)->strempty);
    return (c != EOF);
  }
}

static int io_file_read(lua_State *L, IOFileUD *iof, int start,
			const char *fname, int argshift)
{
  FILE *fp = iof->fp;
  int ok, n, nargs = (int)(L->top - L->base) - start;
#if !LJ_54
  UNUSED(fname);
  UNUSED(argshift);
#endif
  clearerr(fp);
  if (nargs == 0) {
    ok = io_file_readline(L, fp, 1);
    n = start+1;  /* Return 1 result. */
  } else {
    /* The results plus the buffers go on top of the args. */
    luaL_checkstack(L, nargs+LUA_MINSTACK, "too many arguments");
    ok = 1;
    for (n = start; nargs-- && ok; n++) {
      if (tvisstr(L->base+n)) {
	const char *p = strVdata(L->base+n);
	if (p[0] == '*') p++;
	if (p[0] == 'n')
	  ok = io_file_readnum(L, fp);
	else if ((p[0] & ~0x20) == 'L')
	  ok = io_file_readline(L, fp, (p[0] == 'l'));
	else if (p[0] == 'a')
	  io_file_readall(L, fp);
	else
#if LJ_54
	  io_methodargerror54(L, fname, n+1-argshift, "invalid format");
#else
	  lj_err_arg(L, n+1, LJ_ERR_INVFMT);
#endif
      } else if (tvisnumber(L->base+n) || tvisi64(L->base+n)) {
#if LJ_54
	ok = io_file_readlen(L, fp, io_checkreadlen54(L, n+1,
						      n+1-argshift));
#else
	ok = io_file_readlen(L, fp, (MSize)lj_lib_checkint(L, n+1));
#endif
      } else {
#if LJ_54
	io_methodargerror54(L, fname, n+1-argshift,
	  lj_strfmt_pushf(L, "string expected, got %s",
			  io_typename54(L, n+1)));
#else
	lj_err_arg(L, n+1, LJ_ERR_INVOPT);
#endif
      }
    }
  }
  if (ferror(fp))
    return luaL_fileresult(L, 0, NULL);
  if (!ok)
    setnilV(L->top-1);  /* Replace last result with nil. */
  return n - start;
}

static int io_file_write(lua_State *L, IOFileUD *iof, int start,
			 const char *fname)
{
  FILE *fp = iof->fp;
  cTValue *tv;
  int status = 1;
#if !LJ_54
  UNUSED(fname);
#endif
  for (tv = L->base+start; tv < L->top; tv++) {
    MSize len;
    const char *p = lj_strfmt_wstrnum(L, tv, &len);
    if (!p)
#if LJ_54
      io_methodargerror54(L, fname, (int)(tv - L->base) + 1 - start,
	lj_strfmt_pushf(L, "string expected, got %s",
			io_typename54(L, (int)(tv - L->base) + 1)));
#else
      lj_err_argt(L, (int)(tv - L->base) + 1, LUA_TSTRING);
#endif
    status = status && (fwrite(p, 1, len, fp) == len);
  }
  if (LJ_52 && status) {
    L->top = L->base+1;
    if (start == 0)
      setudataV(L, L->base, IOSTDF_UD(L, GCROOT_IO_OUTPUT));
    return 1;
  }
  return luaL_fileresult(L, status, NULL);
}

static int io_file_iter(lua_State *L)
{
  GCfunc *fn = curr_func(L);
  IOFileUD *iof = uddata(udataV(&fn->c.upvalue[0]));
  int n = fn->c.nupvalues - 1;
  if (iof->fp == NULL) {
#if LJ_54
    if (iof->type & IOFILE_FLAG_CLOSE)
      lj_err_callermsg(L, "file is already closed");
#endif
    lj_err_caller(L, LJ_ERR_IOCLFL);
  }
  L->top = L->base;
  if (n) {  /* Copy upvalues with options to stack. */
#if LJ_54
    int packed = 0;
    if (n == 1 && tvistab(&fn->c.upvalue[1])) {
      int i;
      lua_rawgeti(L, lua_upvalueindex(2), 0);
      packed = tvisint(L->top-1);
      n = packed ? (int)intV(L->top-1) : n;
      lua_pop(L, 1);
      if (packed) {
	luaL_checkstack(L, n+LUA_MINSTACK, "too many arguments");
	for (i = 1; i <= n; i++)
	  lua_rawgeti(L, lua_upvalueindex(2), i);
      }
    }
    if (!packed)
#endif
    {
      lj_state_checkstack(L, (MSize)n);
      memcpy(L->top, &fn->c.upvalue[1], n*sizeof(TValue));
      L->top += n;
    }
  }
#if LJ_54
  /* io.lines()/file:lines() iterators have an implicit file argument in
  ** official diagnostics, so copied read options start at public argument #2.
  */
  n = io_file_read(L, iof, 0, "?", -1);
#else
  n = io_file_read(L, iof, 0, "read", 0);
#endif
  if (ferror(iof->fp))
    lj_err_callermsg(L, strVdata(L->top-2));
#if LJ_54
  if (n > 0 && tvisnil(L->top - n) && (iof->type & IOFILE_FLAG_CLOSE)) {
#else
  if (tvisnil(L->base) && (iof->type & IOFILE_FLAG_CLOSE)) {
#endif
    /* With explicit io.lines read options, the copied options stay below the
    ** actual results. Close auto-opened files based on the first result slot,
    ** matching Lua 5.4's EOF test for line iterators.
    */
    io_file_close(L, iof);  /* Return values are ignored. */
    return 0;
  }
  return n;
}

static int io_file_lines(lua_State *L)
{
  int n = (int)(L->top - L->base);
#if LJ_54
  int optn = n - 1;
  if (optn > 250)
    return luaL_error(L, "too many arguments");
  if (n > LJ_MAX_UPVAL) {
    int i;
    /* LuaJIT C closures are limited to LJ_MAX_UPVAL, while Lua 5.4 allows up
    ** to 250 read options. Store all options in a single table upvalue.
    */
    lua_createtable(L, optn, 1);
    for (i = 1; i <= optn; i++) {
      lua_pushvalue(L, i+1);
      lua_rawseti(L, -2, i);
    }
    lua_pushinteger(L, optn);
    lua_rawseti(L, -2, 0);
    lua_pushvalue(L, 1);
    lua_insert(L, -2);
    lua_pushcclosure(L, io_file_iter, 2);
    return 1;
  }
#else
  if (n > LJ_MAX_UPVAL)
    lj_err_caller(L, LJ_ERR_UNPACK);
#endif
  lua_pushcclosure(L, io_file_iter, n);
  return 1;
}

/* -- I/O file methods ---------------------------------------------------- */

#define LJLIB_MODULE_io_method

LJLIB_CF(io_method_close)
{
  IOFileUD *iof;
#if LJ_54
  iof = io_method_file_named54(L, "close");
#else
  if (L->base < L->top) {
    iof = io_tofile(L);
  } else {
    iof = IOSTDF_IOF(L, GCROOT_IO_OUTPUT);
    if (iof->fp == NULL)
      lj_err_caller(L, LJ_ERR_IOCLFL);
  }
#endif
  return io_file_close(L, iof);
}

LJLIB_CF(io_method_read)
{
#if LJ_54
  return io_file_read(L, io_method_file_named54(L, "read"), 1, "read", 1);
#else
  return io_file_read(L, io_tofile(L), 1, "read", 1);
#endif
}

LJLIB_CF(io_method_write)		LJLIB_REC(io_write 0)
{
#if LJ_54
  return io_file_write(L, io_method_file_named54(L, "write"), 1, "write");
#else
  return io_file_write(L, io_tofile(L), 1, "write");
#endif
}

LJLIB_CF(io_method_flush)		LJLIB_REC(io_flush 0)
{
#if LJ_54
  return luaL_fileresult(L, fflush(io_method_file_named54(L, "flush")->fp) == 0,
			 NULL);
#else
  return luaL_fileresult(L, fflush(io_tofile(L)->fp) == 0, NULL);
#endif
}

#if LJ_32 && defined(__ANDROID__) && __ANDROID_API__ < 24
/* The Android NDK is such an unmatched marvel of engineering. */
extern int fseeko32(FILE *, long int, int) __asm__("fseeko");
extern long int ftello32(FILE *) __asm__("ftello");
#define fseeko(fp, pos, whence)	(fseeko32((fp), (pos), (whence)))
#define ftello(fp)		(ftello32((fp)))
#endif

LJLIB_CF(io_method_seek)
{
#if LJ_54
  FILE *fp = io_method_file_named54(L, "seek")->fp;
#else
  FILE *fp = io_tofile(L)->fp;
#endif
#if LJ_54
  int opt = io_method_checkopt54(L, "seek", 2, 1, "\3set\3cur\3end");
#else
  int opt = lj_lib_checkopt(L, 2, 1, "\3set\3cur\3end");
#endif
  int64_t ofs = 0;
#if !LJ_54
  TValue *o;
#endif
  int res;
  if (opt == 0) opt = SEEK_SET;
  else if (opt == 1) opt = SEEK_CUR;
  else if (opt == 2) opt = SEEK_END;
#if LJ_54
  ofs = io_checkseekofs54(L);
#else
  o = L->base+2;
  if (o < L->top) {
    if (tvisstr(o)) lj_strscan_num(strV(o), o);
    if (tvisint(o))
      ofs = (int64_t)intV(o);
    else if (tvisnum(o))
      ofs = lj_num2i64(numV(o));
    else if (!tvisnil(o))
      lj_err_argt(L, 3, LUA_TNUMBER);
  }
#endif
#if LJ_TARGET_POSIX
  res = fseeko(fp, ofs, opt);
#elif _MSC_VER >= 1400
  res = _fseeki64(fp, ofs, opt);
#elif defined(__MINGW32__)
  res = fseeko64(fp, ofs, opt);
#else
  res = fseek(fp, (long)ofs, opt);
#endif
  if (res)
    return luaL_fileresult(L, 0, NULL);
#if LJ_TARGET_POSIX
  ofs = ftello(fp);
#elif _MSC_VER >= 1400
  ofs = _ftelli64(fp);
#elif defined(__MINGW32__)
  ofs = ftello64(fp);
#else
  ofs = (int64_t)ftell(fp);
#endif
  lj_obj_setint64(L, L->top-1, ofs);
  return 1;
}

LJLIB_CF(io_method_setvbuf)
{
#if LJ_54
  FILE *fp = io_method_file_named54(L, "setvbuf")->fp;
#else
  FILE *fp = io_tofile(L)->fp;
#endif
#if LJ_54
  int opt = io_method_checkopt54(L, "setvbuf", 2, -1, "\4full\4line\2no");
#else
  int opt = lj_lib_checkopt(L, 2, -1, "\4full\4line\2no");
#endif
#if LJ_54
  size_t sz = io_checksetvbufsize54(L);
#else
  size_t sz = (size_t)lj_lib_optint(L, 3, LUAL_BUFFERSIZE);
#endif
  if (opt == 0) opt = _IOFBF;
  else if (opt == 1) opt = _IOLBF;
  else if (opt == 2) opt = _IONBF;
  return luaL_fileresult(L, setvbuf(fp, NULL, opt, sz) == 0, NULL);
}

LJLIB_CF(io_method_lines)
{
#if LJ_54
  io_method_file_named54(L, "lines");
#else
  io_tofile(L);
#endif
  return io_file_lines(L);
}

LJLIB_CF(io_method___gc)
{
  IOFileUD *iof = io_tofilep(L);
  if (iof->fp != NULL && (iof->type & IOFILE_TYPE_MASK) != IOFILE_TYPE_STDF)
    io_file_close(L, iof);
  return 0;
}

LJLIB_CF(io_method___tostring)
{
  IOFileUD *iof = io_tofilep(L);
  if (iof->fp != NULL)
    lua_pushfstring(L, "file (%p)", iof->fp);
  else
    lua_pushliteral(L, "file (closed)");
  return 1;
}

LJLIB_PUSH(top-1) LJLIB_SET(__index)

#include "lj_libdef.h"

/* -- I/O library functions ----------------------------------------------- */

#define LJLIB_MODULE_io

LJLIB_PUSH(top-2) LJLIB_SET(!)  /* Set environment. */

LJLIB_CF(io_open)
{
#if LJ_54
  const char *fname = strdata(io_checkstr_named54(L, 1, "io.open"));
  GCstr *s = io_optstr_named54(L, 2, "io.open");
  const char *mode = io_checkmode(L, 2, s, "r", "io.open");
#else
  const char *fname = strdata(lj_lib_checkstr(L, 1));
  GCstr *s = lj_lib_optstr(L, 2);
  const char *mode = s ? strdata(s) : "r";
#endif
  IOFileUD *iof = io_file_new(L);
  iof->fp = fopen(fname, mode);
  return iof->fp != NULL ? 1 : luaL_fileresult(L, 0, fname);
}

LJLIB_CF(io_popen)
{
#if LJ_TARGET_POSIX || (LJ_TARGET_WINDOWS && !LJ_TARGET_XBOXONE && !LJ_TARGET_UWP)
#if LJ_54
  const char *fname = strdata(io_checkstr_named54(L, 1, "io.popen"));
  GCstr *s = io_optstr_named54(L, 2, "io.popen");
  const char *mode = io_checkmode(L, 2, s, "r", "io.popen");
#else
  const char *fname = strdata(lj_lib_checkstr(L, 1));
  GCstr *s = lj_lib_optstr(L, 2);
  const char *mode = s ? strdata(s) : "r";
#endif
  IOFileUD *iof = io_file_new(L);
  iof->type = IOFILE_TYPE_PIPE;
#if LJ_TARGET_POSIX
  fflush(NULL);
  iof->fp = popen(fname, mode);
#else
  iof->fp = _popen(fname, mode);
#endif
  return iof->fp != NULL ? 1 : luaL_fileresult(L, 0, fname);
#else
  return luaL_error(L, LUA_QL("popen") " not supported");
#endif
}

LJLIB_CF(io_tmpfile)
{
  IOFileUD *iof = io_file_new(L);
#if LJ_TARGET_PS3 || LJ_TARGET_PS4 || LJ_TARGET_PS5 || LJ_TARGET_PSVITA || LJ_TARGET_NX
  iof->fp = NULL; errno = ENOSYS;
#else
  iof->fp = tmpfile();
#endif
  return iof->fp != NULL ? 1 : luaL_fileresult(L, 0, NULL);
}

LJLIB_CF(io_close)
{
  IOFileUD *iof;
  if (L->base < L->top) {
#if LJ_54
    iof = io_file_named54(L, "io.close");
#else
    iof = io_tofile(L);
#endif
  } else {
    iof = IOSTDF_IOF(L, GCROOT_IO_OUTPUT);
    if (iof->fp == NULL)
      lj_err_caller(L, LJ_ERR_IOCLFL);
  }
  return io_file_close(L, iof);
}

LJLIB_CF(io_read)
{
  return io_file_read(L, io_stdfile(L, GCROOT_IO_INPUT), 0, "io.read", 0);
}

LJLIB_CF(io_write)		LJLIB_REC(io_write GCROOT_IO_OUTPUT)
{
  return io_file_write(L, io_stdfile(L, GCROOT_IO_OUTPUT), 0, "io.write");
}

LJLIB_CF(io_flush)		LJLIB_REC(io_flush GCROOT_IO_OUTPUT)
{
  return luaL_fileresult(L, fflush(io_stdfile(L, GCROOT_IO_OUTPUT)->fp) == 0, NULL);
}

static int io_std_getset(lua_State *L, ptrdiff_t id, const char *mode
#if LJ_54
			 , const char *fname
#endif
)
{
  if (L->base < L->top && !tvisnil(L->base)) {
    if (tvisudata(L->base)) {
#if LJ_54
      io_file_named54(L, fname);
#else
      io_tofile(L);
#endif
      L->top = L->base+1;
    } else {
#if LJ_54
      /* Lua 5.4 accepts file handles or path strings here; other objects must
      ** report the FILE* expectation so __name-based diagnostics stay intact.
      */
      if (!tvisstr(L->base) && !tvisnumber(L->base) && !tvisi64(L->base))
	io_argtype54(L, fname, 1, LUA_FILEHANDLE);
      (void)io_checkstr_named54(L, 1, fname);
#endif
      io_file_open(L, mode);
    }
    /* NOBARRIER: The standard I/O handles are GC roots. */
    setgcref(G(L)->gcroot[id], gcV(L->top-1));
  } else {
    setudataV(L, L->top++, IOSTDF_UD(L, id));
  }
  return 1;
}

LJLIB_CF(io_input)
{
  return io_std_getset(L, GCROOT_IO_INPUT, "r"
#if LJ_54
		       , "io.input"
#endif
  );
}

LJLIB_CF(io_output)
{
  return io_std_getset(L, GCROOT_IO_OUTPUT, "w"
#if LJ_54
		       , "io.output"
#endif
  );
}

LJLIB_CF(io_lines)
{
#if LJ_54
  GCudata *closing = NULL;
#endif
  if (L->base == L->top) setnilV(L->top++);
  if (!tvisnil(L->base)) {  /* io.lines(fname) */
#if LJ_54
    (void)io_checkstr_named54(L, 1, "io.lines");
#endif
    IOFileUD *iof = io_file_open(L, "r");
    iof->type = IOFILE_TYPE_FILE|IOFILE_FLAG_CLOSE;
#if LJ_54
    closing = udataV(L->top-1);
#endif
    L->top--;
    setudataV(L, L->base, udataV(L->top));
  } else {  /* io.lines() iterates over stdin. */
    setudataV(L, L->base, IOSTDF_UD(L, GCROOT_IO_INPUT));
  }
#if LJ_54
  if (closing) {
    /* Lua 5.4 returns the file as the generic-for closing value. The iterator
    ** still closes it on EOF until full VM-level <close> support is added.
    */
    io_file_lines(L);
    setnilV(L->top++);
    setnilV(L->top++);
    setudataV(L, L->top++, closing);
    return 4;
  }
#endif
  return io_file_lines(L);
}

LJLIB_CF(io_type)
{
#if LJ_54
  cTValue *o = io_checkany54(L, "io.type", 1);
#else
  cTValue *o = lj_lib_checkany(L, 1);
#endif
  if (!(tvisudata(o) && udataV(o)->udtype == UDTYPE_IO_FILE))
    setnilV(L->top++);
  else if (((IOFileUD *)uddata(udataV(o)))->fp != NULL)
    lua_pushliteral(L, "file");
  else
    lua_pushliteral(L, "closed file");
  return 1;
}

#include "lj_libdef.h"

/* ------------------------------------------------------------------------ */

static GCobj *io_std_new(lua_State *L, FILE *fp, const char *name)
{
  IOFileUD *iof = (IOFileUD *)lua_newuserdata(L, sizeof(IOFileUD));
  GCudata *ud = udataV(L->top-1);
  ud->udtype = UDTYPE_IO_FILE;
  /* NOBARRIER: The GCudata is new (marked white). */
  setgcref(ud->metatable, gcV(L->top-3));
  iof->fp = fp;
  iof->type = IOFILE_TYPE_STDF;
  lua_setfield(L, -2, name);
  return obj2gco(ud);
}

LUALIB_API int luaopen_io(lua_State *L)
{
  LJ_LIB_REG(L, NULL, io_method);
#if LJ_54
  /* Lua 5.4 exposes file handles as FILE* in user-facing type diagnostics. */
  lua_pushliteral(L, LUA_FILEHANDLE);
  lua_setfield(L, -2, "__name");
  /* File handles are valid to-be-closed values. Official Lua wires __close to
  ** the GC-style close path, so scope exit ignores already closed handles.
  */
  lua_getfield(L, -1, "__gc");
  lua_setfield(L, -2, "__close");
#endif
  copyTV(L, L->top, L->top-1); L->top++;
  lua_setfield(L, LUA_REGISTRYINDEX, LUA_FILEHANDLE);
  LJ_LIB_REG(L, LUA_IOLIBNAME, io);
  setgcref(G(L)->gcroot[GCROOT_IO_INPUT], io_std_new(L, stdin, "stdin"));
  setgcref(G(L)->gcroot[GCROOT_IO_OUTPUT], io_std_new(L, stdout, "stdout"));
  io_std_new(L, stderr, "stderr");
  return 1;
}

