ifdef SWIFTPATH
	SWIFTC=$(SWIFTPATH)/swiftc
	SWIFT=$(SWIFTPATH)/swift
else
	SWIFTC=xcrun -sdk macosx swiftc
	SWIFT=swift
endif

MAIN=main
MODSRC=rat/rat.swift
SRC=$(MODSRC) rat/main.swift
MODNAME=Rat
MODULE=Rat.swiftmodule Rat.swiftdoc 
SHLIB=libRat

all: $(MAIN)
module: $(MODSRC)
clean:
	-rm $(MAIN) $(MODULE) $(MODULE) $(SHLIB).*
$(MAIN): $(SRC)
	$(SWIFTC) $(SRC)
test: $(MAIN)
	prove ./main
$(MODULE): $(MODSRC)
	$(SWIFTC) -emit-library -emit-module $(MODSRC) -module-name $(MODNAME)
repl: $(MODULE)
	$(SWIFT) -I. -L. -lRat
