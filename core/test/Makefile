SRCDIR=src
DEPDIR=src/deps
BLDDIR=obj
ARDIR=../lib
SVCDIR=../../services/lib
BINDIR=bin
archive=libbpamiocore.a
svcarchive=libbpamiosvcs.a

CORELIB = $(ARDIR)/$(archive)
SVCLIB  = $(SVCDIR)/$(svcarchive)

# core objects (s99, iosvcs, mem) reference errmsg/info which live in svcs,
# so the default link order is: core first, then svcs.
LIBS = $(CORELIB) $(SVCLIB)

csource := $(shell find $(SRCDIR) -type f -name *.c)
cobjs   := $(patsubst $(SRCDIR)/%,$(BLDDIR)/%,$(csource:.c=.o))
objects := $(cobjs)
cdeps   := $(patsubst $(SRCDIR)/%,$(DEPDIR)/%,$(csource:.c=.d))
pgms    := $(BINDIR)/basiccreate $(BINDIR)/basicread $(BINDIR)/basicrddir \
           $(BINDIR)/basicalloc  $(BINDIR)/basicenq  $(BINDIR)/basicmem   \
           $(BINDIR)/basicddfreetest
all: $(pgms)

clean:
	rm -rf $(objects) $(pgms) $(DEPDIR)

$(DEPDIR)/%.d: $(SRCDIR)/%.c
	mkdir -p $(DEPDIR) ; \
	rm -f $@; \
	clang -MM -MP $(CORE_CPPFLAGS) $< > $@;

include $(cdeps)

# Archive build rules: delegate to parent Makefiles so archives are always
# up to date before any test binary is linked.
$(ARDIR)/$(archive):
	$(MAKE) -C .. -f Makefile all-core

$(SVCDIR)/$(svcarchive):
	$(MAKE) -C ../../services -f Makefile all-services

# Pattern rule for the 6 binaries whose entry point is a core symbol.
# $< is the .o; LIBS supplies archives in the correct order for the recipe.
# Both archives appear in the prerequisite list so make rebuilds them when stale.
$(BINDIR)/%: $(BLDDIR)/%.o $(CORELIB) $(SVCLIB)
	mkdir -p $(BINDIR) ;
	$(LD) $(LDFLAGS) -o $@ $< $(LIBS) >$@.llst

# basicddfreetest enters via svcs symbols (open_pds_for_read, close_pds).
# bpamio.o is pulled from svcs first and references dcb_init (core), so
# core must follow svcs.  errmsg/info are resolved from the svcs pass already.
$(BINDIR)/basicddfreetest: $(BLDDIR)/basicddfreetest.o $(CORELIB) $(SVCLIB)
	mkdir -p $(BINDIR) ;
	$(LD) $(LDFLAGS) -o $@ $< $(SVCLIB) $(CORELIB) >$@.llst

$(BLDDIR)/%.o: $(SRCDIR)/%.c
	mkdir -p $(BLDDIR) ;
	$(CC) $(CORE_CPPFLAGS) $(CFLAGS) -c -o $@ $<

.PHONY: all clean
