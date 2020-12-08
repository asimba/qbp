GCC_MINGW32_W64_PREFIX = x86_64-w64-mingw32
GCC_TEST:=$(shell g++ --version 2>&1| head -1 | grep -G ".*\..\.")
GCC_MINGW32_W64_TEST:=$(shell $(GCC_MINGW32_W64_PREFIX)-g++ --version 2>&1| head -1 | grep -G ".*\.")

all:
	@if ! test -d ../obj ; then mkdir ../obj ; fi
	@if test -n "$(GCC_TEST)" ;\
	then make -s -f makefile.mk BUILD_STRING="LINUX" ;\
	fi
	@if test -n "$(GCC_MINGW32_W64_TEST)" ;\
	then make -s -f makefile.mk TARGET=$(GCC_MINGW32_W64_PREFIX) BUILD_STRING="WINDOWS" EXEEXT=".exe" ;\
	fi

clean:
	@make -s -f makefile.mk clean
	@make -s -f makefile.mk clean EXEEXT=".exe"

clean-obj:
	@make -s -f makefile.mk clean-obj

.NOEXPORT:
