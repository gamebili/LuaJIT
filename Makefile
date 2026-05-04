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

smoketest:
	$(MAKE) clean
	$(MAKE)
	./src/luajit test/smoke.lua default

smoketest-lua54compat:
	$(MAKE) clean
	$(MAKE) XCFLAGS='-DLUAJIT_ENABLE_LUA54COMPAT'
	./src/luajit test/smoke.lua lua54compat
	out=$$(./src/luajit -e 'warn("@on"); warn("lua54 ", "warning")' 2>&1 >/dev/null) && test "$$out" = "Lua warning: lua54 warning"
	out=$$(./src/luajit -W -e 'warn("lua54 -W warning")' 2>&1 >/dev/null) && test "$$out" = "Lua warning: lua54 -W warning"
	out=$$(./src/luajit -e 'warn("@on"); do local t=setmetatable({}, { __gc=function() error("lua54 gc boom", 0) end }); t=nil end; collectgarbage(); collectgarbage()' 2>&1 >/dev/null) && test "$$out" = "Lua warning: error in __gc (lua54 gc boom)"
	LUA_INIT='error("wrong init")' LUA_INIT_5_4='lua54_init_marker=54' ./src/luajit -e 'assert(lua54_init_marker == 54)'
	LUA_PATH='old/?.lua' LUA_PATH_5_4='v54/?.lua' LUA_CPATH='old/?.dll' LUA_CPATH_5_4='v54/?.dll' ./src/luajit -e 'assert(package.path:match("^v54/%?%.lua")); assert(package.cpath:match("^v54/%?%.dll"))'
	LUA_INIT_5_4='error("noenv init")' LUA_PATH_5_4='bad/?.lua' LUA_CPATH_5_4='bad/?.dll' ./src/luajit -E -e 'assert(not package.path:match("^bad/")); assert(not package.cpath:match("^bad/"))'
	./src/luajit -E -e 'local p,c,sep=package.path,package.cpath,package.config:sub(1,1); if sep=="\\" then assert(p:find("\\lua\\?.lua",1,true)); assert(p:find("\\lua\\?\\init.lua",1,true)); assert(p:find("..\\share\\lua\\5.4\\?.lua",1,true)); assert(p:find(".\\?\\init.lua",1,true)); assert(c:find("..\\lib\\lua\\5.4\\?.dll",1,true)); assert(c:find(".\\?.dll",1,true)); else assert(p:find("/share/lua/5.4/?.lua",1,true)); assert(p:find("/share/lua/5.4/?/init.lua",1,true)); assert(c:find("/lib/lua/5.4/?.so",1,true)); end'
	./src/luajit -e 'assert(arg[-1] == nil); assert(arg[0]:match("luajit")); assert(arg[1] == "-e"); assert(arg[2]:match("arg%[0%]"))'
	tmp=test/lua54_arg_smoke.tmp; printf 'assert(arg[-1]:match("luajit")); assert(arg[0]:match("lua54_arg_smoke")); assert(arg[1] == "a"); assert(arg[2] == "b")\n' > $$tmp && ./src/luajit $$tmp a b && rm -f $$tmp
	tmp=test/lua54_arg_smoke.tmp; printf 'assert(arg[-2]:match("luajit")); assert(arg[-1] == "--"); assert(arg[0]:match("lua54_arg_smoke")); assert(arg[1] == "a"); assert(arg[2] == "b")\n' > $$tmp && ./src/luajit -- $$tmp a b && rm -f $$tmp
	printf 'assert(arg[-1]:match("luajit")); assert(arg[0] == "-"); assert(arg[1] == "a"); assert(arg[2] == "b")\n' | ./src/luajit - a b
	./src/luajit -l lua54math=math -e 'assert(lua54math.type(1) == "integer")'
	out=$$(printf 'os.exit()\n' | ./src/luajit -i 2>&1) && case "$$out" in *"JIT:"*) exit 1;; esac

smoketest-capi-lua54compat: smoketest-lua54compat
	gcc -DLUAJIT_ENABLE_LUA54COMPAT -std=c99 -I src -c test/lua54_lua_header_smoke.c -o src/lua54_lua_header_smoke.o
	rm -f src/lua54_lua_header_smoke.o
	gcc -DLUAJIT_ENABLE_LUA54COMPAT -std=c99 -I src -c test/lua54_lualib_header_smoke.c -o src/lua54_lualib_header_smoke.o
	rm -f src/lua54_lualib_header_smoke.o
	gcc -DLUAJIT_ENABLE_LUA54COMPAT -I src -x c test/lua54_capi_smoke.c -x none src/lua51.dll -o src/lua54_capi_smoke.exe
	./src/lua54_capi_smoke.exe
	rm -f src/lua54_capi_smoke.exe
	gcc -DLUAJIT_ENABLE_LUA54COMPAT -DLUA_COMPAT_APIINTCASTS -I src -x c test/lua54_capi_intcasts_smoke.c -x none src/lua51.dll -o src/lua54_capi_intcasts_smoke.exe
	./src/lua54_capi_intcasts_smoke.exe
	rm -f src/lua54_capi_intcasts_smoke.exe
	@if gcc -DLUAJIT_ENABLE_LUA54COMPAT -std=c99 -Werror=implicit-function-declaration -I src -c test/lua54_capi_legacy_reject.c -o src/lua54_capi_legacy_reject.o 2>src/lua54_capi_legacy_reject.err; then echo "legacy lauxlib API unexpectedly visible in Lua 5.4 headers"; rm -f src/lua54_capi_legacy_reject.o src/lua54_capi_legacy_reject.err; exit 1; else grep -E "luaL_(openlib|register|pushmodule)" src/lua54_capi_legacy_reject.err >/dev/null; rm -f src/lua54_capi_legacy_reject.o src/lua54_capi_legacy_reject.err; fi
	@if gcc -DLUAJIT_ENABLE_LUA54COMPAT -std=c99 -Werror=implicit-function-declaration -I src -c test/lua54_capi_typerror_reject.c -o src/lua54_capi_typerror_reject.o 2>src/lua54_capi_typerror_reject.err; then echo "luaL_typerror unexpectedly visible in Lua 5.4 headers"; rm -f src/lua54_capi_typerror_reject.o src/lua54_capi_typerror_reject.err; exit 1; else grep "luaL_typerror" src/lua54_capi_typerror_reject.err >/dev/null; rm -f src/lua54_capi_typerror_reject.o src/lua54_capi_typerror_reject.err; fi
	@if gcc -DLUAJIT_ENABLE_LUA54COMPAT -std=c99 -Werror=implicit-function-declaration -I src -c test/lua54_capi_findtable_reject.c -o src/lua54_capi_findtable_reject.o 2>src/lua54_capi_findtable_reject.err; then echo "luaL_findtable unexpectedly visible in Lua 5.4 headers"; rm -f src/lua54_capi_findtable_reject.o src/lua54_capi_findtable_reject.err; exit 1; else grep "luaL_findtable" src/lua54_capi_findtable_reject.err >/dev/null; rm -f src/lua54_capi_findtable_reject.o src/lua54_capi_findtable_reject.err; fi
	@if gcc -DLUAJIT_ENABLE_LUA54COMPAT -std=c99 -Werror=implicit-function-declaration -I src -c test/lua54_capi_setlevel_reject.c -o src/lua54_capi_setlevel_reject.o 2>src/lua54_capi_setlevel_reject.err; then echo "lua_setlevel unexpectedly visible in Lua 5.4 headers"; rm -f src/lua54_capi_setlevel_reject.o src/lua54_capi_setlevel_reject.err; exit 1; else grep "lua_setlevel" src/lua54_capi_setlevel_reject.err >/dev/null; rm -f src/lua54_capi_setlevel_reject.o src/lua54_capi_setlevel_reject.err; fi
	@for sym in PREPBUFFER ARGEXPECTED PUSHFAIL LOADFILE LOADBUFFER; do if gcc -DLUAJIT_ENABLE_LUA54COMPAT -DLUA54_REJECT_$$sym -std=c99 -I src -c test/lua54_capi_lauxlib_macroonly_reject.c -o src/lua54_capi_lauxlib_macroonly_reject.o 2>src/lua54_capi_lauxlib_macroonly_reject.err; then echo "lauxlib macro-only API $$sym unexpectedly has a function address in Lua 5.4 headers"; rm -f src/lua54_capi_lauxlib_macroonly_reject.o src/lua54_capi_lauxlib_macroonly_reject.err; exit 1; else grep "luaL_" src/lua54_capi_lauxlib_macroonly_reject.err >/dev/null; rm -f src/lua54_capi_lauxlib_macroonly_reject.o src/lua54_capi_lauxlib_macroonly_reject.err; fi; done
	@for sym in BIT JIT FFI STRING_BUFFER; do if gcc -DLUAJIT_ENABLE_LUA54COMPAT -DLUA54_REJECT_$$sym -std=c99 -Werror=implicit-function-declaration -I src -c test/lua54_lualib_extra_reject.c -o src/lua54_lualib_extra_reject.o 2>src/lua54_lualib_extra_reject.err; then echo "LuaJIT lualib API $$sym unexpectedly visible in Lua 5.4 headers"; rm -f src/lua54_lualib_extra_reject.o src/lua54_lualib_extra_reject.err; exit 1; else grep "luaopen_" src/lua54_lualib_extra_reject.err >/dev/null; rm -f src/lua54_lualib_extra_reject.o src/lua54_lualib_extra_reject.err; fi; done

smoketest-capi-default: smoketest
	gcc -I src -x c test/lua51_capi_smoke.c -x none src/lua51.dll -o src/lua51_capi_smoke.exe
	./src/lua51_capi_smoke.exe
	rm -f src/lua51_capi_smoke.exe

test: smoketest-capi-default smoketest-capi-lua54compat

.PHONY: all install amalg clean smoketest smoketest-lua54compat smoketest-capi-default smoketest-capi-lua54compat test

##############################################################################
