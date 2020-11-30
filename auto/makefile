include build/config.mk
include build/test_*.mk

.PHONY: install uninstall clean distclean test coverage

VPATH = src/

SRCS:=src/rie_event.c     \
      src/rie_log.c       \
      src/rie_window.c    \
      src/rie_skin.c      \
      src/rie_render.c    \
      src/rie_gfx_cairo.c \
      src/rie_external.c  \
      src/rieman.c        \
      src/rie_config.c    \
      src/rie_xcb.c       \
      src/rie_util.c      \
      src/rie_font.c      \
      src/rie_control.c

ifeq ($(DEBUG),yes)
    # for readable cores
    CFLAGS += -O0
    # for backtraces
    LDFLAGS += -rdynamic
else
    CFLAGS += -O2
endif

ifeq ($(TESTS),yes)
    LIBS += -lpthread
    SRCS += src/rie_test.c
endif

ifeq ($(COVERAGE),yes)
    CFLAGS += -fprofile-arcs -ftest-coverage
    LIBS += -lgcov
endif

DEPFLAGS = -MT $@ -MMD -MP -MF build/$*.Td
POSTCOMPILE = mv -f build/$*.Td build/$*.d

src-to-obj = $(subst src/,build/,$(subst .c,.o,$(filter %.c,$1)))
src-to-dep = $(subst src/,build/,$(subst .c,.d,$(filter %.c,$1)))

# intentionally simplistic, don't expected to work everywhere
OS:=$(shell uname -r -o -m)

# if building from a cloned sources, revision is embedded
REV:=$(shell if [ -d .git ]; then git rev-parse HEAD; else echo "pkg"; fi)

build/config.h: $(testbins)
	@echo "#ifndef __CONFIG_H__" > $@
	@echo "#define __CONFIG_H__" >> $@
	@cat build/*.res >> $@
	@if [ x"$(DEBUG)" = x"yes" ]; then echo "#define RIE_DEBUG" >> $@; fi
	@if [ x"$(TESTS)" = x"yes" ]; then echo "#define RIE_TESTS" >> $@; fi
	@echo "#define RIE_PREFIX  \"$(PREFIX)\"" >> $@
	@echo "#define RIE_BINDIR  \"$(BINDIR)\"" >> $@
	@echo "#define RIE_MANDIR  \"$(MANDIR)\"" >> $@
	@echo "#define RIE_DOCDIR  \"$(DOCDIR)\"" >> $@
	@echo "#define RIE_DATADIR  \"$(DATADIR)\"" >> $@
	@echo "#define RIE_CC \"$(CC)\"" >> $@
	@echo "#define RIE_CFLAGS \"$(CFLAGS)\"" >> $@
	@echo "#define RIE_LDFLAGS \"$(LDFLAGS)\"" >> $@
	@echo "#define RIE_BUILD_OS \"$(OS)\"" >> $@
	@echo "#define RIE_REV \"rev.$(REV)\"" >> $@
	@echo "#endif" >> $@

build/%.d: ;
.PRECIOUS: build/%.d

build/%.o: %.c build/config.h build/%.d
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@
	@$(POSTCOMPILE)

build/rieman: $(call src-to-obj,$(SRCS))
	$(LINK.c) $^ $(LDFLAGS) $(LOADLIBES) $(LDLIBS) -o $@ $(LIBS)
	@echo "--- $(notdir $@) built ok ---"

distclean:
	@rm -rf build rieman-$(VER).tar.gz rieman.log
	@rm makefile
	@echo "--- full cleanup done ---"

clean:
	@rm -f build/rie*
	@echo "--- cleanup done ---"

install: build/rieman
	mkdir -p $(DESTDIR)$(BINDIR)
	mkdir -p $(DESTDIR)$(DATADIR)
	mkdir -p $(DESTDIR)$(MANDIR)
	mkdir -p $(DESTDIR)$(DOCDIR)
	install -m 755 ./build/rieman $(DESTDIR)$(BINDIR)
	install -m 644 ./conf/rieman.xml $(DESTDIR)$(DATADIR)/rieman.xml
	install -m 644 ./conf/rieman-1.0.xsd $(DESTDIR)$(DATADIR)/rieman-1.0.xsd
	install -m 644 ./conf/rieman-1.1.xsd $(DESTDIR)$(DATADIR)/rieman-1.1.xsd
	install -m 644 ./conf/rieman-1.2.xsd $(DESTDIR)$(DATADIR)/rieman-1.2.xsd
	tar cf - ./skins | (cd $(DESTDIR)$(DATADIR); tar xf -)
	install -m 644 ./doc/rieman.1 $(DESTDIR)$(MANDIR)
	install -m 644 ./README.md $(DESTDIR)$(DOCDIR)
	install -m 644 ./COPYING $(DESTDIR)$(DOCDIR)

uninstall:
	rm -i $(DESTDIR)$(BINDIR)/rieman
	rm -i $(DESTDIR)$(MANDIR)/rieman.1
	rm -ir $(DESTDIR)$(DATADIR)
	rm -ir $(DESTDIR)$(DOCDIR)

test: build/rieman
	@echo "--- check if binary has tests enabled ---"
	@if ! ./build/rieman -vv | grep "tests:   yes" > /dev/null ; then     \
	    printf "%s\n" " - Builtin tests are not enabled:";                \
	    printf "%s\n" " please add '--with-tests' option to configure";   \
	    exit 1;                                                           \
	fi
	@echo "--- running external tests ---"
	@python ./tests/rieman_tests.py
	@echo "--- running internal tests ---"
	@RIE_TEST_ENABLE=1 ./build/rieman; true

coverage: build/rieman
	@lcov --directory build --zerocounters > /dev/null 2>&1
	@echo " + coverage counters zeroed, starting tests..."
	lcov -b . -c -i -d ./build -o build/lcov.base
	@python tests/rieman_tests.py
	@echo " + running internal tests..."
	@RIE_TEST_ENABLE=1 ./build/rieman 2>&1 | egrep -v "\-...-"; true
	@echo " + collecting coverage..."
	@lcov --directory build -b build --capture -o build/lcov.run
	@lcov -a build/lcov.base -a build/lcov.run -o build/lcov.total
	@lcov -e build/lcov.total "`pwd`/*" -o build/lcov.filtered
	@lcov -r build/lcov.filtered src/rie_test.c -o build/lcov.final
	@genhtml -o build/report build/lcov.final > build/genhtml.log 2>&1
	@echo " + coverage report ready: 'build/report/index.html'"

# --- Tarball, named for a current version ---

TARBALL=auto conf doc pkg skins src tests README.md COPYING

VER=$(shell sed -n 's/.*RIEMAN_VERSION.*"\(.*\)"/\1/p' src/rieman.h)

tarball: rieman-$(VER).tar.gz

rieman-$(VER).tar.gz: $(TARBALL)
	@echo "-- creating tarball for version $(VER) --"
	@tar --transform 's,^,rieman-$(VER)/,' -c -v -z -f $@ $^
	@echo "-- $@ created ok --"

-include $(call src-to-dep,$(SRCS))
