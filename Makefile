default: release_debug

.PHONY: default release debug all clean cppcheck

include make-utils/flags.mk
include make-utils/cpp-utils.mk

CXX_FLAGS += -pedantic
CXX_FLAGS += -Iinclude -Ilib/rapidxml/

$(eval $(call auto_folder_compile,src))
$(eval $(call auto_add_executable,cov_clean))

release: release_cov_clean
release_debug: release_debug_cov_clean
debug: debug_cov_clean

all: release release_debug debug

cppcheck:
	cppcheck --enable=all --std=c++11 -I include src

prefix = /usr
bindir = ${prefix}/bin

install: release_debug
	@ echo "Installation of coverage_cleaner"
	@ echo "==============================="
	@ echo ""
	install release_debug/bin/cov_clean $(bindir)/cov_clean

clean: base_clean

include make-utils/cpp-utils-finalize.mk
