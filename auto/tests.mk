build/test_cc: auto/tests/test_cc.c
	@printf "%s" "Checking for working C compiler..."
	@if $(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) > $@.log 2>&1; \
    then                                                   \
            echo "ok";                                     \
    else                                                   \
            echo "no"; exit 1;                             \
    fi

# test of CFLAGS and LDFLAGS combination
# $(call tests-cflags,name,cflags,ldflags)
define test-compiler-flags
  testbins += build/$1.mk

  build/$1.mk: auto/tests/$1.c
	@if $(CC) $(CFLAGS) $2 -o build/$1 $$< $(LDFLAGS) $3 > $$@.log 2>&1; \
        then                                                             \
            echo "Checking if compiler supports $2...yes";               \
            echo "CFLAGS += $2" > $$@;                                   \
            echo "LDFLAGS += $3" >> $$@;                                 \
     else                                                                \
            echo "Checking if compiler supports $2...no, ignored";       \
     fi
	@touch $$@
endef


# test for optional library: show message if not passed
# $(call lib-test-opt,name, title,cflags,ldflags,defname)
define lib-test-opt

  testbins += build/$1.mk

  build/$1.mk: auto/tests/$1.c
	@if $(CC) $(CFLAGS) $3 -o $$@ $$< $(LDFLAGS) $4 > $$@.log 2>&1;        \
     then                                                                  \
            echo "Checking for $2...found";                                \
            echo "CFLAGS += $3" > $$@;                                     \
            echo "LIBS += $4" >> $$@;                                      \
            echo "#define RIE_HAVE_$5" > build/$1.res;                     \
     else                                                                  \
            echo "Checking for $2...not found, related features disabled"; \
     fi
	@touch $$@
endef


pcf=$(shell pkg-config --cflags $1 2>/dev/null)
plf=$(shell pkg-config --libs $1 2>/dev/null)

# test for required library with pkg-confi: fails if not passed
# $(call pkg-test,autotest,pkg,define)
define pkg-test

  testbins += build/$1.mk

  build/$1.mk: auto/tests/$1.c
	@if $(CC) $(CFLAGS) $(call pcf,$2) -o $$@ $$< $(LDFLAGS) $(call plf,$2) \
                                                          > $$@.log 2>&1;   \
     then                                                                   \
            echo "Checking for pkg '$2'...found";                           \
            echo "CFLAGS += $(call pcf,$2)" > $$@;                          \
            echo "LIBS += $(call plf,$2)" >> $$@;                           \
     else                                                                   \
            echo "Checking for pkg '$2'...fail";                            \
            false;                                                          \
     fi
	@echo "#define RIE_HAVE_$3" > $$@.res
endef
