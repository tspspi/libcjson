include ../Makefile.SUPPORTED

ifeq (,$(findstring $(OS),$(SUPPORTEDPLATFORMS)))

all:

	@echo The OS environment variable is currently set to [$(OS)]
	@echo Please set the OS environment variable to one of the following:
	@echo $(SUPPORTEDPLATFORMS)

.PHONY: all

else

include ../Makefile.$(OS)

OPTIONS=

SLIBFILE=../bin/libcjson$(SLIBSUFFIX)
LIBHFILES=../include/cjson.h

TESTBINFILES=../bin/tests/test001_parser$(EXESUFFIX) \
	../bin/tests/test002_Serialize$(EXESUFFIX)

all: $(TESTBINFILES)

../bin/tests/%$(EXESUFFIX): %.c $(LIBHFILES) $(SLIBFILE)

	$(CCLIB) $(OPTIONS) -L../bin/ -o $@ $< -lcjson

endif
