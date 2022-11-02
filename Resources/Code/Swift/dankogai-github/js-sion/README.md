[![ES2015](https://img.shields.io/badge/JavaScript-ES2015-blue.svg)](http://www.ecma-international.org/ecma-262/6.0/)
[![MIT LiCENSE](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![CI via GitHub Actions](https://github.com/dankogai/js-sion/actions/workflows/node.js.yml/badge.svg)](https://github.com/dankogai/js-sion/actions/workflows/node.js.yml)

# js-sion

[SION] deserializer/serializer for ECMAScript

[SION]: https://dankogai.github.io/SION/

## Synopsis

```javascript
import {SION} from './sion.js';
//...
let obj = SION.parse('["formats":["JSON","SION"]]');
let str = SION.stringify({formats: ["JSON", "SION"]});
//...
```

## Usage

[sion.js] has no dependency so you can simply put it anywhere handy.  It is a [ES6 module] so you need a faily modern environments.

[sion.js]: ./sion.js
[ES6 module]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Statements/import

### on browsers

In your JS script:

```javascript
import {SION} from './sion.js'; // or wherever you put it
```

Or in your HTML:

```html
<script type="module">
  import {SION} from './sion.js';
</script>
```

You can even directly `import` from CDN:

```html
<script type="module">
  import {SION} from 'https://cdn.jsdelivr.net/npm/js-sion@1.2.0/sion.min.js';
</script>
```

Tree-shaken import is also supported.

```javascript
import {stringify, parse} from './sion.js';
```

Besides `SION`, the trunk, the follow symbols are exported:

* `RE_HEXFLOAT`:
  a `RegExp` that matches hexadecimal floating-point number.
* `RE_HEXFLOAT_G`:
  same as `RE_HEXFLOAT` with a 'g' flag.
* `parseHexFloat`:
  parses hexadecimal floating point.
* `toHexString`:
  prints number in hexadecimal floating point format.
* `stringify`:
  cf. `JSON.stringify`. stringifies a JS object to a SION string.
* `parse`:
  cf. `JSON.parse`. parses SION string to a JS object.
* `version`:
  The version of module.

### on node.js

Use node 16 or later that support native esm.  You can also use use [standard-things/esm].

[standard-things/esm]: https://github.com/standard-things/esm


```sh
$ npm install esm js-sion
$ node
> const SION = await import('js-sion');
undefined
> SION
[Module: null prototype] {
  RE_HEXFLOAT: /([+-]?)0x([0-9A-F]+).?([0-9A-F]*)p([+-]?[0-9]+)/i,
  RE_HEXFLOAT_G: /([+-]?)0x([0-9A-F]+).?([0-9A-F]*)p([+-]?[0-9]+)/gi,
  SION: {
    version: '1.2.0',
    RE_HEXFLOAT: /([+-]?)0x([0-9A-F]+).?([0-9A-F]*)p([+-]?[0-9]+)/i,
    RE_HEXFLOAT_G: /([+-]?)0x([0-9A-F]+).?([0-9A-F]*)p([+-]?[0-9]+)/gi,
    parseHexFloat: [Function: parseHexFloat],
    toHexString: [Function: toHexString],
    stringify: [Function: stringify],
    parse: [Function: parse]
  },
  parse: [Function: parse],
  parseHexFloat: [Function: parseHexFloat],
  stringify: [Function: stringify],
  toHexString: [Function: toHexString],
  version: '1.2.0'
}
> SION.parse('["formats":["JSON","SION"]]');
{ formats: [ 'JSON', 'SION' ] }
>  SION.stringify({formats: ["JSON", "SION"]});
'["formats":["JSON","SION"]]'
```
