include Makefile.SUPPORTED

ifeq (,$(findstring $(OS),$(SUPPORTEDPLATFORMS)))

all:

	@echo The OS environment variable is currently set to [$(OS)]
	@echo Please set the OS environment variable to one of the following:
	@echo $(SUPPORTEDPLATFORMS)

.PHONY: all

else

include Makefile.$(OS)

OPTIONS=
LIBSRCFILES=src/cjson.c \
	src/cjsonArray.c \
	src/cjsonBoolNull.c \
	src/cjsonNumber.c \
	src/cjsonObject.c \
	src/cjsonParser.c \
	src/cjsonSerializer.c \
	src/cjsonString.c

LIBHFILES=include/cjson.h

OBJFILES=tmp/cjson$(OBJSUFFIX) \
	tmp/cjsonArray$(OBJSUFFIX) \
	tmp/cjsonBoolNull$(OBJSUFFIX) \
	tmp/cjsonNumber$(OBJSUFFIX) \
	tmp/cjsonObject$(OBJSUFFIX) \
	tmp/cjsonParser$(OBJSUFFIX) \
	tmp/cjsonSerializer$(OBJSUFFIX) \
	tmp/cjsonString$(OBJSUFFIX)

all: staticlib tests

staticlib: $(OBJFILES)

	$(ARCMD) bin/libcjson$(SLIBSUFFIX) $(OBJFILES)

tests:

	- @$(MAKE) -C ./tests

package: staticlib

	# Create staging hierarchy
	$(MKDIR) stage/lib
	$(MKDIR) packages

	# Copy files
	$(CP) bin/libcjson$(SLIBSUFFIX) stage/lib/$(SLIBSUFFIX)

	# Create archive
	$(MKSTAGEARCHIVE)

	# Cleanup
	$(RMDIR) stage

clean:

	- @$(RMDIR) stage
	- @$(RMDIR) packages
	- @$(RMFILE) bin/*$(SLIBSUFFIX)
	- @$(RMFILE) bin/tests/test*

.PHONY: staticlib tests clean

tmp/%$(OBJSUFFIX): src/%.c $(LIBHFILES)

	$(CCLIB) $(OPTIONS) -c -o $@ $<

endif
