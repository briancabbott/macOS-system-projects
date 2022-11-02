import {SION} from '../sion.js';
describe('Load', () => {
  it('SION is an object', () => 
    chai.expect(typeof SION).to.equal('object')
  );
  it('SION.parse is a function', () =>
    chai.expect(typeof SION.parse).to.equal('function')
  );
  it('SION.stringify is a function', () => 
    chai.expect(typeof SION.stringify).to.equal('function')
  );
});
let map = new Map();
map.set(null,    'nil');
map.set(true,    'true');
map.set(false,   'false');
map.set(42,      '42');
map.set(-42.195, '-0x1.518f5c28f5c29p+5');
map.set(new Date(0.0), '.Date(0x0p+0)');
map.set(Uint32Array.of(0xdeadbeef), '.Data("776t3g==")');
map.set([0,[1,2,[3,4,5]]], '[0,[1,2,[3,4,5]]]');
map.set(
  {"zero":0,"one":{"two":2,"three":3}}, '["zero":0,"one":["two":2,"three":3]]'
);
map.set(new Map([[false,0],[true,1]]), '[false:0,true:1]');

describe('Stringify', () => {
  const typeName = (obj) => {
    return obj == null ? 'null' 
      : Object.prototype.toString.call(obj).replace(/\[object (.*)\]/, '$1');
  };
  for (var [k, v] of map) {
    it(typeName(k) + ': ' + JSON.stringify(k), 
      () => chai.expect(SION.stringify(k)).to.equal(v));
  }
});
describe('Parse', () => {
  let obj = {}
  for (var [k, v] of map) {
    obj[v] = k;
  }
  for (k in obj) {
    it(k, () => chai.expect(SION.parse(k)).to.deep.equal(obj[k]))
  }
});
