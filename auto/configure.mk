include build/config.mk

testbins :=
include auto/tests.mk

ifeq ($(ENABLE_ASAN),yes)
$(eval $(call test-compiler-flags,test_asan,$(ASAN_CFLAGS),$(ASAN_LDFLAGS)))
endif

$(eval $(call pkg-test,test_x11_lib,x11,X11))
$(eval $(call pkg-test,test_xlib_xcb_lib,x11-xcb,XLIB_XCB))
$(eval $(call pkg-test,test_xcb_lib,xcb,XCB))
$(eval $(call pkg-test,test_xcb-util_lib,xcb-util,XCB_UTIL))
$(eval $(call pkg-test,test_xcb-icccm_lib,xcb-icccm,XCB_ICCCM))
$(eval $(call pkg-test,test_xcb-ewmh_lib,xcb-ewmh,XCB_EWMH))
$(eval $(call pkg-test,test_cairo_lib,cairo,CAIRO))
$(eval $(call pkg-test,test_fc_lib,fontconfig,FONTCONFIG))
$(eval $(call pkg-test,test_ft_lib,freetype2,FREETYPE))
$(eval $(call pkg-test,test_xml_lib,libxml-2.0,XML))

result=$(if $(findstring yes,$1),$2: yes,$2: no)
oresult=$(if $1,$2: $1)

$(testbins): build/test_cc

makefile: $(testbins)
	@cp auto/makefile $@
	@echo ""
	@echo "--- rieman configured succesfuly ---"
	@echo "    prefix: $(PREFIX)"
	@echo "    $(call result,$(TESTS),tests)"
	@echo "    $(call result,$(DEBUG),debug)"
	@echo "    $(call result,$(COVERAGE),coverage)"
	@echo "    $(call oresult,$(CC),compiler)"
	@echo "    $(call result,$(ENABLE_ASAN),enable asan)"

