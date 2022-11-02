PJ=package.json
TS=sion.ts
JS=sion.js
MJS=sion.mjs
DTS=sion.d.ts

all: $(PJ) $(JS)

$(JS): $(PJ) $(TS)
	tsc -d --target es6 $(TS)

test: $(PJ) $(JS)
	mocha

clean:
	-rm $(DTS) $(MJS) $(JS)
