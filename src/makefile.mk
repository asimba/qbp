SOURCES =   rc32.cc \
            nvoc.cc \
            lzss.cc \
            qbpmain.cc

PROGRAM = qbp

ifdef TARGET
  CC = $(TARGET)-g++
else
  CC = g++
endif

ARCH = x86-64
TUNE = generic

CFLAGS_T = -Wall -march=$(ARCH) -mtune=$(TUNE) -O2 -fno-exceptions -static -s

CFLAGS = $(CFLAGS_T)

INCLUDES =  -I.
LDFLAGS = -Wall -march=$(ARCH) -mtune=$(TUNE) -O2 -fno-exceptions -static -s
LFLAGS = 

SRCDIR = .
BINDIR = ../bin
OBJDIR = ../obj

OBJEXT = o

OBJECTS=$(addprefix $(OBJDIR)/,$(SOURCES:.cc=.$(OBJEXT)))

all: clean $(PROGRAM)

clean-obj:
	@echo "Cleaning generated build files..."
	@rm -rf $(OBJDIR)/*

clean: clean-obj
	@echo "Cleaning old binaries..."
	@rm -f $(BINDIR)/$(PROGRAM)$(EXEEXT)

.SUFFIXES:
.SUFFIXES: .cc .$(OBJEXT)

$(PROGRAM): $(OBJECTS)
	@echo "Linking   ($(BUILD_STRING)): $(notdir $(BINDIR))/$(@F)$(EXEEXT)"
	@$(CC) $(LDFLAGS) $(OBJECTS) -o $(BINDIR)/$@$(EXEEXT) $(LFLAGS)
	@strip -X -R .note -R .comment -R .note.gnu.build-id -R .note.ABI-tag $(BINDIR)/$@$(EXEEXT)

$(OBJDIR)/%.o:%.cc
	@echo "Compiling ($(BUILD_STRING)): $(notdir $(CURDIR))/$(<F)"
	@$(CC) -c $(CFLAGS) $(INCLUDES) $< -o $@

$(OBJDIR)/%.tmpo:%.cc
	@if ! test -f ./hashtbl.h ; then \
	echo "Compiling ($(BUILD_STRING)): $(notdir $(CURDIR))/$(<F)" ;\
	$(CC) -c $(CFLAGS) $(INCLUDES) $< -o $@ ; fi

.NOEXPORT:
