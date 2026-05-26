/*
** Load and dump code.
** Copyright (C) 2005-2026 Mike Pall. See Copyright Notice in luajit.h
*/

#include <errno.h>
#include <stdio.h>

#define lj_load_c
#define LUA_CORE

#include "lua.h"
#include "lauxlib.h"

#include "lj_obj.h"
#include "lj_gc.h"
#include "lj_err.h"
#include "lj_buf.h"
#include "lj_func.h"
#include "lj_frame.h"
#include "lj_vm.h"
#include "lj_lex.h"
#include "lj_bcdump.h"
#include "lj_parse.h"

/* -- Load Lua source code and bytecode ----------------------------------- */

static TValue *cpparser(lua_State *L, lua_CFunction dummy, void *ud)
{
  LexState *ls = (LexState *)ud;
  GCproto *pt;
  GCfunc *fn;
  int bc;
  UNUSED(dummy);
  cframe_errfunc(L->cframe) = -1;  /* Inherit error function. */
  bc = lj_lex_setup(L, ls);
  if (ls->mode) {
    int xmode = 1;
    const char *mode = ls->mode;
    char c;
    while ((c = *mode++)) {
      if (c == (bc ? 'b' : 't')) xmode = 0;
      if (c == (LJ_FR2 ? 'W' : 'X')) ls->fr2 = !LJ_FR2;
    }
    if (xmode) {
#if LJ_54
      /* Lua 5.4 reports which chunk kind was rejected and echoes the mode
      ** string, which helps callers diagnose text/binary loading mistakes.
      */
      lua_pushfstring(L, "attempt to load a %s chunk (mode is '%s')",
		      bc ? "binary" : "text", ls->mode);
#else
      setstrV(L, L->top++, lj_err_str(L, LJ_ERR_XMODE));
#endif
      lj_err_throw(L, LUA_ERRSYNTAX);
    }
  }
  pt = bc ? lj_bcread(ls) : lj_parse(ls);
  if (ls->fr2 == LJ_FR2) {
    fn = lj_func_newL_empty(L, pt, tabref(L->env));
#if LJ_54
    lj_func_inituv_tabenv(L, fn, tabref(L->env));
#endif
    /* Don't combine above/below into one statement. */
    setfuncV(L, L->top++, fn);
  } else {
    /* Non-native generation returns a dumpable, but non-runnable prototype. */
    setprotoV(L, L->top++, pt);
  }
  return NULL;
}

LUA_API int lua_loadx(lua_State *L, lua_Reader reader, void *data,
		      const char *chunkname, const char *mode)
{
  LexState ls;
  int status;
  if (reader == NULL)
    lj_err_msg(L, LJ_ERR_BADVAL);
  ls.rfunc = reader;
  ls.rdata = data;
  ls.chunkarg = chunkname ? chunkname : "?";
  ls.mode = mode;
  lj_buf_init(L, &ls.sb);
  status = lj_vm_cpcall(L, NULL, &ls, cpparser);
  lj_lex_cleanup(L, &ls);
  lj_gc_check(L);
  return status;
}

#if LJ_54
LUA_API int lua_load54(lua_State *L, lua_Reader reader, void *data,
		       const char *chunkname, const char *mode)
{
  /* Keep LuaJIT's internal lua_load/lua_loadx ABI stable while external
  ** Lua 5.4 modules see the official five-argument lua_load surface.
  */
  return lua_loadx(L, reader, data, chunkname, mode);
}
#endif

LUA_API int lua_load(lua_State *L, lua_Reader reader, void *data,
		     const char *chunkname)
{
  return lua_loadx(L, reader, data, chunkname, NULL);
}

typedef struct FileReaderCtx {
  FILE *fp;
  char buf[LUAL_BUFFERSIZE];
  size_t n;
  size_t pos;
} FileReaderCtx;

static const char *reader_file(lua_State *L, void *ud, size_t *size)
{
  FileReaderCtx *ctx = (FileReaderCtx *)ud;
  UNUSED(L);
  if (ctx->pos < ctx->n) {
    const char *p = ctx->buf + ctx->pos;
    *size = ctx->n - ctx->pos;
    ctx->pos = ctx->n;
    return p;
  }
  if (feof(ctx->fp)) return NULL;
  *size = fread(ctx->buf, 1, sizeof(ctx->buf), ctx->fp);
  return *size > 0 ? ctx->buf : NULL;
}

#if LJ_54
static int loadfile_skipbom(FILE *fp)
{
  int c = getc(fp);
  if (c == 0xef && getc(fp) == 0xbb && getc(fp) == 0xbf)
    return getc(fp);
  return c;
}

static int loadfile_skipcomment(FILE *fp, int *cp)
{
  int c = *cp = loadfile_skipbom(fp);
  if (c == '#') {
    do {
      c = getc(fp);
    } while (c != EOF && c != '\n');
    *cp = getc(fp);
    return 1;
  }
  return 0;
}
#endif

LUALIB_API int luaL_loadfilex(lua_State *L, const char *filename,
			      const char *mode)
{
  FileReaderCtx ctx;
  int status;
  const char *chunkname;
  int err = 0;
#if LJ_54
  int c;
  int skipped;
#endif
  if (filename) {
    chunkname = lua_pushfstring(L, "@%s", filename);
    ctx.fp = fopen(filename, "rb");
    if (ctx.fp == NULL) {
      L->top--;
      lua_pushfstring(L, "cannot open %s: %s", filename, strerror(errno));
      return LUA_ERRFILE;
    }
  } else {
    ctx.fp = stdin;
    chunkname = "=stdin";
  }
  ctx.n = ctx.pos = 0;
#if LJ_54
  skipped = loadfile_skipcomment(ctx.fp, &c);
  if (skipped)
    ctx.buf[ctx.n++] = '\n';
  /* Lua 5.4 accepts a first-line # comment before binary chunks. LuaJIT opens
  ** files in binary mode already, so just drop the line-number padding before
  ** feeding the bytecode signature to the loader.
  */
  if (c == BCDUMP_HEAD1)
    ctx.n = 0;
  if (c != EOF)
    ctx.buf[ctx.n++] = (char)c;
#endif
  status = lua_loadx(L, reader_file, &ctx, chunkname, mode);
  if (ferror(ctx.fp)) err = errno;
  if (filename) {
    fclose(ctx.fp);
    L->top--;
    copyTV(L, L->top-1, L->top);
  }
  if (err) {
    const char *fname = filename ? filename : "stdin";
    L->top--;
    lua_pushfstring(L, "cannot read %s: %s", fname, strerror(err));
    return LUA_ERRFILE;
  }
  return status;
}

LUALIB_API int luaL_loadfile(lua_State *L, const char *filename)
{
  return luaL_loadfilex(L, filename, NULL);
}

typedef struct StringReaderCtx {
  const char *str;
  size_t size;
} StringReaderCtx;

static const char *reader_string(lua_State *L, void *ud, size_t *size)
{
  StringReaderCtx *ctx = (StringReaderCtx *)ud;
  UNUSED(L);
  if (ctx->size == 0) return NULL;
  *size = ctx->size;
  ctx->size = 0;
  return ctx->str;
}

LUALIB_API int luaL_loadbufferx(lua_State *L, const char *buf, size_t size,
				const char *name, const char *mode)
{
  StringReaderCtx ctx;
  ctx.str = buf;
  ctx.size = size;
  return lua_loadx(L, reader_string, &ctx, name, mode);
}

LUALIB_API int luaL_loadbuffer(lua_State *L, const char *buf, size_t size,
			       const char *name)
{
  return luaL_loadbufferx(L, buf, size, name, NULL);
}

LUALIB_API int luaL_loadstring(lua_State *L, const char *s)
{
  return luaL_loadbuffer(L, s, strlen(s), s);
}

/* -- Dump bytecode ------------------------------------------------------- */

static int dump_lua_func(lua_State *L, lua_Writer writer, void *data,
			 uint32_t flags)
{
  cTValue *o = L->top-1;
  if (L->top <= L->base)
    lj_err_msg(L, LJ_ERR_BADVAL);
  if (writer == NULL)
    lj_err_msg(L, LJ_ERR_BADVAL);
  lj_checkapi(L->top > L->base, "top slot empty");
  if (tvisfunc(o) && isluafunc(funcV(o)))
    return lj_bcwrite(L, funcproto(funcV(o)), writer, data, flags);
  else
    return 1;
}

LUA_API int lua_dump(lua_State *L, lua_Writer writer, void *data)
{
  uint32_t flags = LJ_FR2*BCDUMP_F_FR2;  /* Default mode for legacy C API. */
  return dump_lua_func(L, writer, data, flags);
}

#if LJ_54
LUA_API int lua_dump54(lua_State *L, lua_Writer writer, void *data, int strip)
{
  uint32_t flags = LJ_FR2*BCDUMP_F_FR2;
  /* Lua 5.4 exposes the strip choice as an API argument; LuaJIT still writes
  ** LuaJIT bytecode, but forwards the flag to the bytecode writer.
  */
  if (strip)
    flags |= BCDUMP_F_STRIP;
  return dump_lua_func(L, writer, data, flags);
}
#endif

