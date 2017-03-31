SRCDIR = src
TESTSRCDIR = $(SRCDIR)/test
LIBSRCDIR = $(SRCDIR)/lib
BUILDDIR = build

LIBH = $(wildcard $(LIBSRCDIR)/*.h)
LIBSRC = $(wildcard $(LIBSRCDIR)/*.c)
MAINSRC := $(wildcard $(SRCDIR)/*.c)
LIBOBJ := $(LIBSRC:$(LIBSRCDIR)/%.c=$(BUILDDIR)/%.o)
MAINOBJ := $(MAINSRC:$(SRCDIR)/%.c=$(BUILDDIR)/%.o)
LIBNAME = $(BUILDDIR)/libhairgap.a

WIREHAIR = ./wirehair
LIBWIREHAIR = $(WIREHAIR)/bin/libwirehair.a
LIBWIREHAIR_DEBUG = $(WIREHAIR)/bin/libwirehair_debug.a

CC = clang
OPTFLAGS = -Ofast -D_FORTIFY_SOURCE=1
DBGFLAGS = -g -O0 -DDEBUG
IFLAGS = -I$(WIREHAIR)/include -I$(LIBSRCDIR)
GPRFLAGS = -pg -g
CFLAGS = -Wall -Wextra -fPIE -fstack-protector-strong \
	 -Wno-format-security \
	 -Werror \
         $(IFLAGS)
LDFLAGS = $(IFLAGS) -L$(WIREHAIR)/bin -lpthread -lstdc++ -Wl,-z,now -Wl,-z,relro

INSTALLDIR=/usr/local
BIN=${INSTALLDIR}/bin

all: release

test: debug hgap_test channel_test
	./hgap_test
	./channel_test
	./test/integration_tests.sh

doc: Doxyfile ${LIBSRC} ${MAINSRC} ${LIBH}
	doxygen


# Installation

install: release
	@echo "Installing to '${INSTALLDIR}'..."
	install -d $(BIN)
	install -m755 hairgaps hairgapr $(BIN)
	@echo "Done."

uninstall:
	@echo "Removing from '${INSTALLDIR}'..."
	rm  ${BIN}/hairgaps ${BIN}/hairgapr
	@echo "Done."


# Targets

dirs: build

build:
	mkdir -p build

debug: CFLAGS += $(DBGFLAGS)
debug: LIBWIREHAIR_CHOSEN := $(LIBWIREHAIR_DEBUG)
debug: dirs $(LIBWIREHAIR_DEBUG) hairgaps hairgapr

profile: CFLAGS += $(GPRFLAGS)
profile: LDFLAGS += -pg
profile: dirs release

release: CFLAGS += -D_FORTIFY_SOURCE=1 $(OPTFLAGS)
release: LIBWIREHAIR_CHOSEN := $(LIBWIREHAIR)
release: dirs $(LIBWIREHAIR) hairgaps hairgapr

clean:
	-rm -r build
	-cd wirehair && make clean

dist-clean: clean
	-rm -r hairgaps hairgapr channel_test hgap_test doc/*


# Compilation

$(LIBNAME): $(LIBOBJ)
	ar rcs $(LIBNAME) $^

hairgapr: $(BUILDDIR)/hairgapr.o $(LIBNAME)
	$(CC) $^ -o $@ $(LDFLAGS) $(LIBWIREHAIR_CHOSEN)

hairgaps: $(BUILDDIR)/hairgaps.o $(LIBNAME)
	$(CC) $^ -o $@ $(LDFLAGS) $(LIBWIREHAIR_CHOSEN)

$(BUILDDIR)/hairgapr.o: $(LIBSRCDIR)/proto.h
$(BUILDDIR)/hairgaps.o: $(LIBSRCDIR)/proto.h
$(BUILDDIR)/hairproto.o: $(LIBSRCDIR)/proto.h
$(BUILDDIR)/bufpool.o: $(LIBSRCDIR)/bufpool.h

$(LIBOBJ): $(BUILDDIR)/%.o:$(LIBSRCDIR)/%.c
	$(CC) -c $< -o $@ $(CFLAGS)

$(MAINOBJ): $(BUILDDIR)/%.o:$(SRCDIR)/%.c
	$(CC) -c $< -o $@ $(CFLAGS)

hgap_test: $(TESTSRCDIR)/hgap_test.c $(LIBNAME)
	$(CC) $^ -o $@ $(CFLAGS) $(LDFLAGS) $(LIBWIREHAIR_DEBUG)

channel_test: CFLAGS += $(OPTFLAGS)
channel_test: $(TESTSRCDIR)/channel_test.c $(LIBSRCDIR)/channel.c \
              $(LIBSRCDIR)/common.c
	$(CC) $^ -o $@ $(CFLAGS) $(LDFLAGS)

$(LIBWIREHAIR):
	cd $(WIREHAIR) && make clean && make

$(LIBWIREHAIR_DEBUG):
	cd $(WIREHAIR) && make clean && make debug
