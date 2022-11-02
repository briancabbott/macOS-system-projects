BUILDDIR=.build/debug
BIN=$(BUILDDIR)/main
RESULTS=$(BUILDDIR)/Results.txt
MOD=TAP
MODSRC=Sources/TAP/tap.swift
MODULE=$(BUILDDIR)/$(MOD).swiftmodule $(BUILDDIR)/$(MOD).swiftdoc
SAMPLESRC=Sources/TAPSample/main.swift
TESTSSRC=Sources/Tests/TAPTests/TAPTests.swift
SWIFTC=swiftc
SWIFT=swift
ifdef SWIFTPATH
	SWIFTC=$(SWIFTPATH)/swiftc
	SWIFT=$(SWIFTPATH)/swift
endif

all: $(BIN)

$(BIN): $(MODSRC) $(SAMPLESRC)
	$(SWIFT) build
clean:
	-rm -r .build
$(RESULTS): $(BIN) $(TESTSSRC)
	$(SWIFT) test | grep -v "^(Compile|Linking)" | cat > $(RESULTS) && echo
test: $(RESULTS)
	prove --exec "cat" $(RESULTS)
	@echo
	prove ./$(BIN)
repl: $(MODULE)
	$(SWIFT) -I$(BUILDDIR) -L$(BUILDDIR) -l$(BUILDDIR)/$(MOD)
