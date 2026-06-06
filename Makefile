##############################################################################
# LuaJIT top level Makefile for installation. Requires GNU Make.
#
# Please read doc/install.html before changing any variables!
#
# Suitable for POSIX platforms (Linux, *BSD, OSX etc.).
# Note: src/Makefile has many more configurable options.
#
# ##### This Makefile is NOT useful for Windows! #####
# For MSVC, please follow the instructions given in src/msvcbuild.bat.
# For MinGW and Cygwin, cd to src and run make with the Makefile there.
#
# Copyright (C) 2005-2026 Mike Pall. See Copyright Notice in luajit.h
##############################################################################

MAJVER=  2
MINVER=  1
ABIVER=  5.1

# LuaJIT uses rolling releases. The release version is based on the time of
# the latest git commit. The 'git' command must be available during the build.
RELVER= $(shell cat src/luajit_relver.txt 2>/dev/null || : )
# Note: setting it with := doesn't work, since it will change during the build.

MMVERSION= $(MAJVER).$(MINVER)
VERSION= $(MMVERSION).$(RELVER)

##############################################################################
#
# Change the installation path as needed. This automatically adjusts
# the paths in src/luaconf.h, too. Note: PREFIX must be an absolute path!
#
export PREFIX= /usr/local
export MULTILIB= lib
LUA54_SRC_DIR?= D:/p4_gl2/pristine/tools/lua/lua-5.4.8-src/lua-5.4.8
LUA54_TESTES_DIR?= $(LUA54_SRC_DIR)/testes
LUA54COMPAT_XCFLAGS= -DLUAJIT_ENABLE_LUA54COMPAT -DLUAJIT_NUMMODE=2
LUA54COMPAT_BUILD_STAMP= src/.luajit-build-config
CXX?= g++
##############################################################################

DPREFIX= $(DESTDIR)$(PREFIX)
INSTALL_BIN=   $(DPREFIX)/bin
INSTALL_LIB=   $(DPREFIX)/$(MULTILIB)
INSTALL_SHARE_= $(PREFIX)/share
INSTALL_SHARE= $(DESTDIR)$(INSTALL_SHARE_)
INSTALL_DEFINC= $(DPREFIX)/include/luajit-$(MMVERSION)
INSTALL_INC=   $(INSTALL_DEFINC)

export INSTALL_LJLIBD= $(INSTALL_SHARE_)/luajit-$(MMVERSION)
INSTALL_JITLIB= $(DESTDIR)$(INSTALL_LJLIBD)/jit
INSTALL_LMODD= $(INSTALL_SHARE)/lua
INSTALL_LMOD= $(INSTALL_LMODD)/$(ABIVER)
INSTALL_CMODD= $(INSTALL_LIB)/lua
INSTALL_CMOD= $(INSTALL_CMODD)/$(ABIVER)
INSTALL_MAN= $(INSTALL_SHARE)/man/man1
INSTALL_PKGCONFIG= $(INSTALL_LIB)/pkgconfig

INSTALL_TNAME= luajit-$(VERSION)
INSTALL_TSYMNAME= luajit
INSTALL_ANAME= libluajit-$(ABIVER).a
INSTALL_SOSHORT1= libluajit-$(ABIVER).so
INSTALL_SOSHORT2= libluajit-$(ABIVER).so.$(MAJVER)
INSTALL_SONAME= libluajit-$(ABIVER).so.$(VERSION)
INSTALL_DYLIBSHORT1= libluajit-$(ABIVER).dylib
INSTALL_DYLIBSHORT2= libluajit-$(ABIVER).$(MAJVER).dylib
INSTALL_DYLIBNAME= libluajit-$(ABIVER).$(VERSION).dylib
INSTALL_PCNAME= luajit.pc

INSTALL_STATIC= $(INSTALL_LIB)/$(INSTALL_ANAME)
INSTALL_DYN= $(INSTALL_LIB)/$(INSTALL_SONAME)
INSTALL_SHORT1= $(INSTALL_LIB)/$(INSTALL_SOSHORT1)
INSTALL_SHORT2= $(INSTALL_LIB)/$(INSTALL_SOSHORT2)
INSTALL_T= $(INSTALL_BIN)/$(INSTALL_TNAME)
INSTALL_TSYM= $(INSTALL_BIN)/$(INSTALL_TSYMNAME)
INSTALL_PC= $(INSTALL_PKGCONFIG)/$(INSTALL_PCNAME)

INSTALL_DIRS= $(INSTALL_BIN) $(INSTALL_LIB) $(INSTALL_INC) $(INSTALL_MAN) \
  $(INSTALL_PKGCONFIG) $(INSTALL_JITLIB) $(INSTALL_LMOD) $(INSTALL_CMOD)
UNINSTALL_DIRS= $(INSTALL_JITLIB) $(DESTDIR)$(INSTALL_LJLIBD) $(INSTALL_INC) \
  $(INSTALL_LMOD) $(INSTALL_LMODD) $(INSTALL_CMOD) $(INSTALL_CMODD)

RM= rm -f
MKDIR= mkdir -p
RMDIR= rmdir 2>/dev/null
SYMLINK= ln -sf
INSTALL_X= install -m 0755
INSTALL_F= install -m 0644
UNINSTALL= $(RM)
LDCONFIG= ldconfig -n 2>/dev/null
SED_PC= sed -e "s|^prefix=.*|prefix=$(PREFIX)|" \
	    -e "s|^multilib=.*|multilib=$(MULTILIB)|" \
	    -e "s|^relver=.*|relver=$(RELVER)|"
ifneq ($(INSTALL_DEFINC),$(INSTALL_INC))
  SED_PC+= -e "s|^includedir=.*|includedir=$(INSTALL_INC)|"
endif

FILE_T= luajit
FILE_A= libluajit.a
FILE_SO= libluajit.so
FILE_MAN= luajit.1
FILE_PC= luajit.pc
FILES_INC= lua.h lualib.h lauxlib.h luaconf.h lua.hpp luajit.h
FILES_JITLIB= bc.lua bcsave.lua dump.lua p.lua v.lua zone.lua \
	      dis_x86.lua dis_x64.lua dis_arm.lua dis_arm64.lua \
	      dis_arm64be.lua dis_ppc.lua dis_mips.lua dis_mipsel.lua \
	      dis_mips64.lua dis_mips64el.lua \
	      dis_mips64r6.lua dis_mips64r6el.lua \
	      vmdef.lua

ifeq (,$(findstring Windows,$(OS)))
  HOST_SYS:= $(shell uname -s)
else
  HOST_SYS= Windows
endif
TARGET_SYS?= $(HOST_SYS)

ifneq (,$(filter $(TARGET_SYS),Darwin iOS))
  INSTALL_SONAME= $(INSTALL_DYLIBNAME)
  INSTALL_SOSHORT1= $(INSTALL_DYLIBSHORT1)
  INSTALL_SOSHORT2= $(INSTALL_DYLIBSHORT2)
  LDCONFIG= :
  SED_PC+= -e "s| -Wl,-E||"
endif

##############################################################################

INSTALL_DEP= src/luajit

default all $(INSTALL_DEP):
	@echo "==== Building LuaJIT $(MMVERSION) ===="
	$(MAKE) -C src
	@echo "==== Successfully built LuaJIT $(MMVERSION) ===="

install: $(INSTALL_DEP)
	@echo "==== Installing LuaJIT $(VERSION) to $(PREFIX) ===="
	$(MKDIR) $(INSTALL_DIRS)
	cd src && $(INSTALL_X) $(FILE_T) $(INSTALL_T)
	cd src && test -f $(FILE_A) && $(INSTALL_F) $(FILE_A) $(INSTALL_STATIC) || :
	$(RM) $(INSTALL_DYN) $(INSTALL_SHORT1) $(INSTALL_SHORT2)
	cd src && test -f $(FILE_SO) && \
	  $(INSTALL_X) $(FILE_SO) $(INSTALL_DYN) && \
	  ( $(LDCONFIG) $(INSTALL_LIB) || : ) && \
	  $(SYMLINK) $(INSTALL_SONAME) $(INSTALL_SHORT1) && \
	  $(SYMLINK) $(INSTALL_SONAME) $(INSTALL_SHORT2) || :
	cd etc && $(INSTALL_F) $(FILE_MAN) $(INSTALL_MAN)
	cd etc && $(SED_PC) $(FILE_PC) > $(FILE_PC).tmp && \
	  $(INSTALL_F) $(FILE_PC).tmp $(INSTALL_PC) && \
	  $(RM) $(FILE_PC).tmp
	cd src && $(INSTALL_F) $(FILES_INC) $(INSTALL_INC)
	cd src/jit && $(INSTALL_F) $(FILES_JITLIB) $(INSTALL_JITLIB)
	$(SYMLINK) $(INSTALL_TNAME) $(INSTALL_TSYM)
	@echo "==== Successfully installed LuaJIT $(VERSION) to $(PREFIX) ===="

uninstall:
	@echo "==== Uninstalling LuaJIT $(VERSION) from $(PREFIX) ===="
	$(UNINSTALL) $(INSTALL_TSYM) $(INSTALL_T) $(INSTALL_STATIC) $(INSTALL_DYN) $(INSTALL_SHORT1) $(INSTALL_SHORT2) $(INSTALL_MAN)/$(FILE_MAN) $(INSTALL_PC)
	for file in $(FILES_JITLIB); do \
	  $(UNINSTALL) $(INSTALL_JITLIB)/$$file; \
	  done
	for file in $(FILES_INC); do \
	  $(UNINSTALL) $(INSTALL_INC)/$$file; \
	  done
	$(LDCONFIG) $(INSTALL_LIB)
	$(RMDIR) $(UNINSTALL_DIRS) || :
	@echo "==== Successfully uninstalled LuaJIT $(VERSION) from $(PREFIX) ===="

##############################################################################

amalg:
	@echo "==== Building LuaJIT $(MMVERSION) (amalgamation) ===="
	$(MAKE) -C src amalg
	@echo "==== Successfully built LuaJIT $(MMVERSION) (amalgamation) ===="

clean:
	$(MAKE) -C src clean
	$(RM) lua54_valid_open_mode.tmp lua54_invalid_open_mode.tmp lua54_loadfile_env.tmp lua54_loadfile_binary_env.tmp test/lua54_capi_hash_binary.tmp $(LUA54COMPAT_BUILD_STAMP)

smoketest:
	$(MAKE) clean
	$(MAKE)
	./src/luajit test/smoke.lua default

build-lua54compat:
	$(MAKE) clean
	$(MAKE) XCFLAGS='$(LUA54COMPAT_XCFLAGS)'
	@printf '%s\n' "$(LUA54COMPAT_XCFLAGS)" > $(LUA54COMPAT_BUILD_STAMP)

build-lua54compat-incremental:
	@cfg='$(LUA54COMPAT_XCFLAGS)'; if test -f $(LUA54COMPAT_BUILD_STAMP) && test "$$(cat $(LUA54COMPAT_BUILD_STAMP))" = "$$cfg"; then echo "==== Reusing Lua 5.4 compatibility build config ===="; else echo "==== Lua 5.4 compatibility build config changed; cleaning ===="; $(MAKE) clean; fi
	$(MAKE) XCFLAGS='$(LUA54COMPAT_XCFLAGS)'
	@printf '%s\n' "$(LUA54COMPAT_XCFLAGS)" > $(LUA54COMPAT_BUILD_STAMP)

smoketest-lua54compat:
	$(MAKE) build-lua54compat
	$(MAKE) run-lua54compat-tests

smoketest-lua54compat-quick:
	$(MAKE) build-lua54compat-incremental
	$(MAKE) run-lua54compat-tests

LUA54_RUNTIME_PARALLEL_TARGETS= \
	lua54-runtime-official-matrix \
	lua54-runtime-cstack-regress \
	lua54-runtime-gc-regress \
	lua54-runtime-jit-regress \
	lua54-runtime-tpack-regress \
	lua54-runtime-stdlib-edges \
	lua54-runtime-vm-backend-static \
	lua54-runtime-vm-backend-dynasm \
	lua54-runtime-smoke \
	lua54-runtime-standalone-regress \
	lua54-runtime-warning-smoke \
	lua54-runtime-package-path-smoke \
	lua54-runtime-arg-smoke \
	lua54-runtime-loadlib-smoke

run-lua54compat-tests: $(LUA54_RUNTIME_PARALLEL_TARGETS)

lua54-runtime-official-matrix:
	MAKEFLAGS= ./src/luajit test/lua54_official_matrix.lua "$(LUA54_TESTES_DIR)"

lua54-runtime-cstack-regress:
	./src/luajit test/lua54_cstack_regress.lua

lua54-runtime-gc-regress:
	./src/luajit test/lua54_gc_regress.lua

lua54-runtime-jit-regress:
	LUA_PATH_5_4='./src/?.lua;./src/?/init.lua;;' ./src/luajit test/lua54_jit_regress.lua

lua54-runtime-tpack-regress:
	./src/luajit test/lua54_tpack_regress.lua

lua54-runtime-stdlib-edges:
	./src/luajit test/lua54_stdlib_edges.lua

lua54-runtime-vm-backend-static:
	./src/luajit test/lua54_vm_backend_static.lua

lua54-runtime-vm-backend-dynasm:
	./src/luajit test/lua54_vm_backend_dynasm.lua

lua54-runtime-smoke:
	LUA_PATH_5_4='./src/?.lua;./src/?/init.lua;;' ./src/luajit test/smoke.lua lua54compat

lua54-runtime-standalone-regress:
	./src/luajit test/lua54_standalone_regress.lua ./src/luajit

lua54-runtime-warning-smoke:
	out=$$(./src/luajit -e 'warn("@on"); warn("lua54 ", "warning")' 2>&1 >/dev/null) && test "$$out" = "Lua warning: lua54 warning"
	out=$$(./src/luajit -W -e 'warn("lua54 -W warning")' 2>&1 >/dev/null) && test "$$out" = "Lua warning: lua54 -W warning"
	out=$$(./src/luajit -e 'warn("lua54 before -W")' -W 2>&1 >/dev/null) && test "$$out" = ""
	out=$$(./src/luajit -W -e 'warn("lua54 after -W")' 2>&1 >/dev/null) && test "$$out" = "Lua warning: lua54 after -W"
	out=$$(./src/luajit -e 'warn("lua54 hidden")' -W -e 'warn("lua54 visible")' 2>&1 >/dev/null) && test "$$out" = "Lua warning: lua54 visible"
	out=$$(./src/luajit -e 'warn("@on"); warn("@off", "XXX", "@off"); warn("@off")' 2>&1 >/dev/null) && test "$$out" = "Lua warning: @offXXX@off"
	out=$$(./src/luajit -e 'warn("@on"); do local t=setmetatable({}, { __gc=function() error("lua54 gc boom", 0) end }); t=nil end; collectgarbage(); collectgarbage()' 2>&1 >/dev/null) && test "$$out" = "Lua warning: error in __gc (lua54 gc boom)"

lua54-runtime-package-path-smoke:
	./src/luajit -E -e 'local p,c,sep=package.path,package.cpath,package.config:sub(1,1); if sep=="\\" then assert(p:find("\\lua\\?.lua",1,true)); assert(p:find("\\lua\\?\\init.lua",1,true)); assert(p:find("..\\share\\lua\\5.4\\?.lua",1,true)); assert(p:find(".\\?\\init.lua",1,true)); assert(c:find("..\\lib\\lua\\5.4\\?.dll",1,true)); assert(c:find(".\\?.dll",1,true)); else assert(p:find("/share/lua/5.4/?.lua",1,true)); assert(p:find("/share/lua/5.4/?/init.lua",1,true)); assert(c:find("/lib/lua/5.4/?.so",1,true)); end'

lua54-runtime-arg-smoke:
	./src/luajit -e 'assert(arg[-1] == nil); assert(arg[0]:match("luajit")); assert(arg[1] == "-e"); assert(arg[2]:match("arg%[0%]"))'
	tmp=test/lua54_arg_smoke.tmp; printf 'assert(arg[-1]:match("luajit")); assert(arg[0]:match("lua54_arg_smoke")); assert(arg[1] == "a"); assert(arg[2] == "b")\n' > $$tmp && ./src/luajit $$tmp a b && rm -f $$tmp
	tmp=test/lua54_arg_smoke.tmp; printf 'assert(arg[-2]:match("luajit")); assert(arg[-1] == "--"); assert(arg[0]:match("lua54_arg_smoke")); assert(arg[1] == "a"); assert(arg[2] == "b")\n' > $$tmp && ./src/luajit -- $$tmp a b && rm -f $$tmp
	printf 'assert(arg[-1]:match("luajit")); assert(arg[0] == "-"); assert(arg[1] == "a"); assert(arg[2] == "b")\n' | ./src/luajit - a b

lua54-runtime-loadlib-smoke:
	./src/luajit -l lua54math=math -e 'assert(lua54math.type(1) == "integer")'

smoketest-lua54compat53:
	$(MAKE) clean
	$(MAKE) XCFLAGS='-DLUAJIT_ENABLE_LUA54COMPAT -DLUAJIT_NUMMODE=2 -DLUA_COMPAT_5_3'
	./src/luajit test/lua54_compat53_runtime.lua

smoketest-lua54compat-nogc64:
	$(MAKE) clean
	$(MAKE) XCFLAGS='-DLUAJIT_ENABLE_LUA54COMPAT -DLUAJIT_NUMMODE=2 -DLUAJIT_DISABLE_GC64'
	./src/luajit -e 'assert(_VERSION == "Lua 5.4"); assert(require("jit").lua54compat == true)'
	LUA_PATH_5_4='./src/?.lua;./src/?/init.lua;;' ./src/luajit test/smoke.lua lua54compat
	LUA_PATH_5_4='./src/?.lua;./src/?/init.lua;;' ./src/luajit test/lua54_jit_regress.lua

run-official-lua54compat:
	MAKEFLAGS= ./src/luajit test/lua54_official_matrix.lua "$(LUA54_TESTES_DIR)"

smoketest-official-lua54compat:
	$(MAKE) clean
	$(MAKE) XCFLAGS='$(LUA54COMPAT_XCFLAGS)'
	MAKEFLAGS= ./src/luajit test/lua54_official_matrix.lua "$(LUA54_TESTES_DIR)"

smoketest-capi-lua54compat: smoketest-lua54compat
	$(MAKE) run-capi-lua54compat-tests

smoketest-capi-lua54compat-quick: smoketest-lua54compat-quick
	$(MAKE) run-capi-lua54compat-tests

LUA54_CAPI_PARALLEL_TARGETS= \
	lua54-capi-luaconf-guard-smoke \
	lua54-capi-luaconf-extra-reject \
	lua54-capi-luaconf-apicheck-smoke \
	lua54-capi-lua-header-smoke \
	lua54-capi-lua-guard-smoke \
	lua54-capi-lauxlib-header-smoke \
	lua54-capi-lauxlib-core-header-smoke \
	lua54-capi-lauxlib-guard-smoke \
	lua54-capi-lualib-header-smoke \
	lua54-capi-lualib-guard-smoke \
	lua54-capi-lu-hpp-header-smoke \
	lua54-capi-header-static \
	lua54-capi-header-macro-audit \
	lua54-capi-runtime-smoke \
	lua54-capi-warning-null-smoke \
	lua54-capi-intcasts-smoke \
	lua54-capi-compat53-smoke \
	lua54-capi-intcasts-reject \
	lua54-capi-intcasts-macroonly-reject \
	lua54-capi-legacy-reject \
	lua54-capi-legacy-lua-api-reject \
	lua54-capi-loadx-reject \
	lua54-capi-typerror-reject \
	lua54-capi-findtable-reject \
	lua54-capi-putchar-reject \
	lua54-capi-setlevel-reject \
	lua54-capi-call-macroonly-reject \
	lua54-capi-macroonly-reject \
	lua54-capi-lauxlib-macroonly-reject \
	lua54-capi-lualib-extra-reject

run-capi-lua54compat-tests: $(LUA54_CAPI_PARALLEL_TARGETS)

lua54-capi-luaconf-guard-smoke:
	gcc -DLUAJIT_ENABLE_LUA54COMPAT -std=c99 -I src -c test/lua54_luaconf_guard_smoke.c -o src/lua54_luaconf_guard_smoke.o
	rm -f src/lua54_luaconf_guard_smoke.o

lua54-capi-luaconf-extra-reject:
	gcc -DLUAJIT_ENABLE_LUA54COMPAT -std=c99 -I src -c test/lua54_luaconf_extra_reject.c -o src/lua54_luaconf_extra_reject.o
	rm -f src/lua54_luaconf_extra_reject.o

lua54-capi-luaconf-apicheck-smoke:
	gcc -DLUAJIT_ENABLE_LUA54COMPAT -std=c99 -I src -c test/lua54_luaconf_apicheck_smoke.c -o src/lua54_luaconf_apicheck_smoke.o
	rm -f src/lua54_luaconf_apicheck_smoke.o

lua54-capi-lua-header-smoke:
	gcc -DLUAJIT_ENABLE_LUA54COMPAT -std=c99 -I src -c test/lua54_lua_header_smoke.c -o src/lua54_lua_header_smoke.o
	rm -f src/lua54_lua_header_smoke.o

lua54-capi-lua-guard-smoke:
	gcc -DLUAJIT_ENABLE_LUA54COMPAT -std=c99 -I src -c test/lua54_lua_guard_smoke.c -o src/lua54_lua_guard_smoke.o
	rm -f src/lua54_lua_guard_smoke.o

lua54-capi-lauxlib-header-smoke:
	gcc -DLUAJIT_ENABLE_LUA54COMPAT -std=c99 -I src -c test/lua54_lauxlib_header_smoke.c -o src/lua54_lauxlib_header_smoke.o
	rm -f src/lua54_lauxlib_header_smoke.o

lua54-capi-lauxlib-core-header-smoke:
	gcc -DLUAJIT_ENABLE_LUA54COMPAT -std=c99 -I src -c test/lua54_lauxlib_core_header_smoke.c -o src/lua54_lauxlib_core_header_smoke.o
	rm -f src/lua54_lauxlib_core_header_smoke.o

lua54-capi-lauxlib-guard-smoke:
	gcc -DLUAJIT_ENABLE_LUA54COMPAT -std=c99 -I src -c test/lua54_lauxlib_guard_smoke.c -o src/lua54_lauxlib_guard_smoke.o
	rm -f src/lua54_lauxlib_guard_smoke.o

lua54-capi-lualib-header-smoke:
	gcc -DLUAJIT_ENABLE_LUA54COMPAT -std=c99 -I src -c test/lua54_lualib_header_smoke.c -o src/lua54_lualib_header_smoke.o
	rm -f src/lua54_lualib_header_smoke.o

lua54-capi-lualib-guard-smoke:
	gcc -DLUAJIT_ENABLE_LUA54COMPAT -std=c99 -I src -c test/lua54_lualib_guard_smoke.c -o src/lua54_lualib_guard_smoke.o
	rm -f src/lua54_lualib_guard_smoke.o

lua54-capi-lu-hpp-header-smoke:
	$(CXX) -DLUAJIT_ENABLE_LUA54COMPAT -std=c++11 -I src -c test/lua54_lu.hpp_header_smoke.cpp -o src/lua54_lu.hpp_header_smoke.o
	rm -f src/lua54_lu.hpp_header_smoke.o

lua54-capi-header-static:
	./src/luajit test/lua54_header_static.lua

lua54-capi-header-macro-audit:
	./src/luajit test/lua54_header_macro_audit.lua "$(LUA54_SRC_DIR)"

lua54-capi-runtime-smoke:
	gcc -DLUAJIT_ENABLE_LUA54COMPAT -I src -x c test/lua54_capi_smoke.c -x none src/lua51.dll -o src/lua54_capi_smoke.exe
	./src/lua54_capi_smoke.exe
	rm -f src/lua54_capi_smoke.exe

lua54-capi-warning-null-smoke:
	gcc -DLUAJIT_ENABLE_LUA54COMPAT -I src -x c test/lua54_capi_warning_null_smoke.c -x none src/lua51.dll -o src/lua54_capi_warning_null_smoke.exe
	./src/lua54_capi_warning_null_smoke.exe 2>src/lua54_capi_warning_null_smoke.err
	test ! -s src/lua54_capi_warning_null_smoke.err
	rm -f src/lua54_capi_warning_null_smoke.exe src/lua54_capi_warning_null_smoke.err

lua54-capi-intcasts-smoke:
	gcc -DLUAJIT_ENABLE_LUA54COMPAT -DLUA_COMPAT_APIINTCASTS -I src -x c test/lua54_capi_intcasts_smoke.c -x none src/lua51.dll -o src/lua54_capi_intcasts_smoke.exe
	./src/lua54_capi_intcasts_smoke.exe
	rm -f src/lua54_capi_intcasts_smoke.exe

lua54-capi-compat53-smoke:
	gcc -DLUAJIT_ENABLE_LUA54COMPAT -DLUA_COMPAT_5_3 -I src -x c test/lua54_capi_compat53_smoke.c -x none src/lua51.dll -o src/lua54_capi_compat53_smoke.exe
	./src/lua54_capi_compat53_smoke.exe
	rm -f src/lua54_capi_compat53_smoke.exe

lua54-capi-intcasts-reject:
	@if gcc -DLUAJIT_ENABLE_LUA54COMPAT -std=c99 -Werror=implicit-function-declaration -I src -c test/lua54_capi_intcasts_reject.c -o src/lua54_capi_intcasts_reject.o 2>src/lua54_capi_intcasts_reject.err; then echo "deprecated intcast macros unexpectedly visible without LUA_COMPAT_APIINTCASTS"; rm -f src/lua54_capi_intcasts_reject.o src/lua54_capi_intcasts_reject.err; exit 1; else grep -E "luaL_(checkint|optint|checklong|optlong|checkunsigned|optunsigned)|lua_(pushunsigned|tounsignedx|tounsigned)" src/lua54_capi_intcasts_reject.err >/dev/null; rm -f src/lua54_capi_intcasts_reject.o src/lua54_capi_intcasts_reject.err; fi

lua54-capi-intcasts-macroonly-reject:
	@for sym in PUSHUNSIGNED TOUNSIGNEDX TOUNSIGNED CHECKUNSIGNED OPTUNSIGNED CHECKINT OPTINT CHECKLONG OPTLONG; do if gcc -DLUAJIT_ENABLE_LUA54COMPAT -DLUA_COMPAT_APIINTCASTS -DLUA54_REJECT_$$sym -std=c99 -I src -c test/lua54_capi_intcasts_macroonly_reject.c -o src/lua54_capi_intcasts_macroonly_reject.o 2>src/lua54_capi_intcasts_macroonly_reject.err; then echo "LUA_COMPAT_APIINTCASTS macro-only API $$sym unexpectedly has a function address"; rm -f src/lua54_capi_intcasts_macroonly_reject.o src/lua54_capi_intcasts_macroonly_reject.err; exit 1; else grep -E "luaL_|lua_" src/lua54_capi_intcasts_macroonly_reject.err >/dev/null; rm -f src/lua54_capi_intcasts_macroonly_reject.o src/lua54_capi_intcasts_macroonly_reject.err; fi; done

lua54-capi-legacy-reject:
	@if gcc -DLUAJIT_ENABLE_LUA54COMPAT -std=c99 -Werror=implicit-function-declaration -I src -c test/lua54_capi_legacy_reject.c -o src/lua54_capi_legacy_reject.o 2>src/lua54_capi_legacy_reject.err; then echo "legacy lauxlib API unexpectedly visible in Lua 5.4 headers"; rm -f src/lua54_capi_legacy_reject.o src/lua54_capi_legacy_reject.err; exit 1; else grep -E "luaL_(openlib|register|pushmodule)" src/lua54_capi_legacy_reject.err >/dev/null; rm -f src/lua54_capi_legacy_reject.o src/lua54_capi_legacy_reject.err; fi

lua54-capi-legacy-lua-api-reject:
	@if gcc -DLUAJIT_ENABLE_LUA54COMPAT -std=c99 -Werror=implicit-function-declaration -I src -c test/lua54_capi_legacy_lua_api_reject.c -o src/lua54_capi_legacy_lua_api_reject.o 2>src/lua54_capi_legacy_lua_api_reject.err; then echo "legacy Lua 5.1 C API unexpectedly visible in Lua 5.4 headers"; rm -f src/lua54_capi_legacy_lua_api_reject.o src/lua54_capi_legacy_lua_api_reject.err; exit 1; else grep -E "lua_(equal|lessthan|strlen|objlen|cpcall|open|getregistry|getgccount|getfenv|setfenv)|lua_Chunk(reader|writer)" src/lua54_capi_legacy_lua_api_reject.err >/dev/null; rm -f src/lua54_capi_legacy_lua_api_reject.o src/lua54_capi_legacy_lua_api_reject.err; fi

lua54-capi-loadx-reject:
	@if gcc -DLUAJIT_ENABLE_LUA54COMPAT -std=c99 -Werror=implicit-function-declaration -I src -c test/lua54_capi_loadx_reject.c -o src/lua54_capi_loadx_reject.o 2>src/lua54_capi_loadx_reject.err; then echo "lua_loadx unexpectedly visible in Lua 5.4 headers"; rm -f src/lua54_capi_loadx_reject.o src/lua54_capi_loadx_reject.err; exit 1; else grep "lua_loadx" src/lua54_capi_loadx_reject.err >/dev/null; rm -f src/lua54_capi_loadx_reject.o src/lua54_capi_loadx_reject.err; fi

lua54-capi-typerror-reject:
	@if gcc -DLUAJIT_ENABLE_LUA54COMPAT -std=c99 -Werror=implicit-function-declaration -I src -c test/lua54_capi_typerror_reject.c -o src/lua54_capi_typerror_reject.o 2>src/lua54_capi_typerror_reject.err; then echo "luaL_typerror unexpectedly visible in Lua 5.4 headers"; rm -f src/lua54_capi_typerror_reject.o src/lua54_capi_typerror_reject.err; exit 1; else grep "luaL_typerror" src/lua54_capi_typerror_reject.err >/dev/null; rm -f src/lua54_capi_typerror_reject.o src/lua54_capi_typerror_reject.err; fi

lua54-capi-findtable-reject:
	@if gcc -DLUAJIT_ENABLE_LUA54COMPAT -std=c99 -Werror=implicit-function-declaration -I src -c test/lua54_capi_findtable_reject.c -o src/lua54_capi_findtable_reject.o 2>src/lua54_capi_findtable_reject.err; then echo "luaL_findtable unexpectedly visible in Lua 5.4 headers"; rm -f src/lua54_capi_findtable_reject.o src/lua54_capi_findtable_reject.err; exit 1; else grep "luaL_findtable" src/lua54_capi_findtable_reject.err >/dev/null; rm -f src/lua54_capi_findtable_reject.o src/lua54_capi_findtable_reject.err; fi

lua54-capi-putchar-reject:
	@if gcc -DLUAJIT_ENABLE_LUA54COMPAT -std=c99 -Werror=implicit-function-declaration -I src -c test/lua54_capi_putchar_reject.c -o src/lua54_capi_putchar_reject.o 2>src/lua54_capi_putchar_reject.err; then echo "luaL_putchar unexpectedly visible in Lua 5.4 headers"; rm -f src/lua54_capi_putchar_reject.o src/lua54_capi_putchar_reject.err; exit 1; else grep "luaL_putchar" src/lua54_capi_putchar_reject.err >/dev/null; rm -f src/lua54_capi_putchar_reject.o src/lua54_capi_putchar_reject.err; fi

lua54-capi-setlevel-reject:
	@if gcc -DLUAJIT_ENABLE_LUA54COMPAT -std=c99 -Werror=implicit-function-declaration -I src -c test/lua54_capi_setlevel_reject.c -o src/lua54_capi_setlevel_reject.o 2>src/lua54_capi_setlevel_reject.err; then echo "lua_setlevel unexpectedly visible in Lua 5.4 headers"; rm -f src/lua54_capi_setlevel_reject.o src/lua54_capi_setlevel_reject.err; exit 1; else grep "lua_setlevel" src/lua54_capi_setlevel_reject.err >/dev/null; rm -f src/lua54_capi_setlevel_reject.o src/lua54_capi_setlevel_reject.err; fi

lua54-capi-call-macroonly-reject:
	@for sym in CALL PCALL YIELD; do if gcc -DLUAJIT_ENABLE_LUA54COMPAT -DLUA54_REJECT_$$sym -std=c99 -I src -c test/lua54_capi_call_macroonly_reject.c -o src/lua54_capi_call_macroonly_reject.o 2>src/lua54_capi_call_macroonly_reject.err; then echo "Lua 5.4 macro-only call API $$sym unexpectedly has a function address"; rm -f src/lua54_capi_call_macroonly_reject.o src/lua54_capi_call_macroonly_reject.err; exit 1; else grep "lua_" src/lua54_capi_call_macroonly_reject.err >/dev/null; rm -f src/lua54_capi_call_macroonly_reject.o src/lua54_capi_call_macroonly_reject.err; fi; done

lua54-capi-macroonly-reject:
	@for sym in GETEXTRASPACE UPVALUEINDEX TONUMBER TOINTEGER NUMBERTOINTEGER PUSHGLOBALTABLE INSERT REMOVE REPLACE NEWUSERDATA GETUSERVALUE SETUSERVALUE POP NEWTABLE REGISTER PUSHCFUNCTION PUSHLITERAL TOSTRING ISFUNCTION ISTABLE ISLIGHTUSERDATA ISNIL ISBOOLEAN ISTHREAD ISNONE ISNONEORNIL; do if gcc -DLUAJIT_ENABLE_LUA54COMPAT -DLUA54_REJECT_$$sym -std=c99 -I src -c test/lua54_capi_macroonly_reject.c -o src/lua54_capi_macroonly_reject.o 2>src/lua54_capi_macroonly_reject.err; then echo "Lua 5.4 macro-only API $$sym unexpectedly has a function address"; rm -f src/lua54_capi_macroonly_reject.o src/lua54_capi_macroonly_reject.err; exit 1; else grep "lua_" src/lua54_capi_macroonly_reject.err >/dev/null; rm -f src/lua54_capi_macroonly_reject.o src/lua54_capi_macroonly_reject.err; fi; done

lua54-capi-lauxlib-macroonly-reject:
	@for sym in PREPBUFFER ADDCHAR ADDSIZE BUFFADDR BUFFLEN BUFFSUB ARGCHECK ARGEXPECTED PUSHFAIL CHECKSTRING OPTSTRING TYPENAME LOADFILE LOADBUFFER DOFILE DOSTRING GETMETATABLE OPT CHECKVERSION INTOP NEWLIBTABLE NEWLIB WRITESTRING WRITELINE WRITESTRINGERROR ASSERT; do if gcc -DLUAJIT_ENABLE_LUA54COMPAT -DLUA54_REJECT_$$sym -std=c99 -I src -c test/lua54_capi_lauxlib_macroonly_reject.c -o src/lua54_capi_lauxlib_macroonly_reject.o 2>src/lua54_capi_lauxlib_macroonly_reject.err; then echo "lauxlib macro-only API $$sym unexpectedly has a function address in Lua 5.4 headers"; rm -f src/lua54_capi_lauxlib_macroonly_reject.o src/lua54_capi_lauxlib_macroonly_reject.err; exit 1; else grep -E "luaL_|lua_" src/lua54_capi_lauxlib_macroonly_reject.err >/dev/null; rm -f src/lua54_capi_lauxlib_macroonly_reject.o src/lua54_capi_lauxlib_macroonly_reject.err; fi; done

lua54-capi-lualib-extra-reject:
	@for sym in BIT BASE54 JIT FFI STRING_BUFFER; do if gcc -DLUAJIT_ENABLE_LUA54COMPAT -DLUA54_REJECT_$$sym -std=c99 -Werror=implicit-function-declaration -I src -c test/lua54_lualib_extra_reject.c -o src/lua54_lualib_extra_reject.o 2>src/lua54_lualib_extra_reject.err; then echo "LuaJIT lualib API $$sym unexpectedly visible in Lua 5.4 headers"; rm -f src/lua54_lualib_extra_reject.o src/lua54_lualib_extra_reject.err; exit 1; else grep "luaopen_" src/lua54_lualib_extra_reject.err >/dev/null; rm -f src/lua54_lualib_extra_reject.o src/lua54_lualib_extra_reject.err; fi; done

smoketest-perf-lua54compat:
	$(MAKE) clean
	$(MAKE) XCFLAGS='$(LUA54COMPAT_XCFLAGS)'
	LUA54_PERF_JIT_OPTS='3,hotloop=3,hotexit=2,instunroll=4,loopunroll=4' ./src/luajit test/lua54_perf.lua jit_on
	LUA54_PERF_JIT_OPTS='3,hotloop=56,hotexit=10' ./src/luajit test/lua54_perf.lua jit_on
	LUA54_PERF_JIT_OPTS='0,hotloop=3,hotexit=2' ./src/luajit test/lua54_perf.lua jit_on
	./src/luajit test/lua54_perf.lua jit_off

smoketest-capi-default: smoketest
	$(CXX) -std=c++11 -I src -c test/lua51_lu.hpp_header_smoke.cpp -o src/lua51_lu.hpp_header_smoke.o
	rm -f src/lua51_lu.hpp_header_smoke.o
	gcc -I src -x c test/lua51_capi_smoke.c -x none src/lua51.dll -o src/lua51_capi_smoke.exe
	./src/lua51_capi_smoke.exe
	rm -f src/lua51_capi_smoke.exe

test:
	$(MAKE) smoketest-capi-default
	$(MAKE) smoketest-capi-lua54compat
	$(MAKE) smoketest-lua54compat-nogc64
	$(MAKE) smoketest-perf-lua54compat

.PHONY: all install amalg clean smoketest build-lua54compat build-lua54compat-incremental smoketest-lua54compat smoketest-lua54compat-quick run-lua54compat-tests $(LUA54_RUNTIME_PARALLEL_TARGETS) smoketest-lua54compat-nogc64 run-official-lua54compat smoketest-official-lua54compat smoketest-capi-default smoketest-capi-lua54compat smoketest-capi-lua54compat-quick run-capi-lua54compat-tests $(LUA54_CAPI_PARALLEL_TARGETS) smoketest-perf-lua54compat test

##############################################################################
