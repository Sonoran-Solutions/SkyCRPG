// Executes the original AS2 routing/movement bodies with native-menu stubs.
// This detects category-first dispatch regressions; it is not a GFx/game test.
const fs = require('node:fs');
const assert = require('node:assert/strict');
const root = process.argv[2];
const methods = fs.readFileSync(root + '/scaleform/grid-methods.as.inc', 'utf8');
const moveSource = methods.slice(methods.indexOf('public function skycrpgMove'), methods.indexOf('public function skycrpgPaintCell'))
  .trim().replace('public function', 'function').replace('details: Object', 'details');
global.Shared = {GlobalFunc: {IsKeyPressed: d => d.value === 'keyDown' || d.value === 'keyHold'}};
global.gfx = {ui: {NavigationCode: {UP:'up', DOWN:'down', LEFT:'left', RIGHT:'right', ENTER:'enter'}}};
const move = eval('(' + moveSource + ')');
const route = process.argv.includes('--legacy-category-first') ? '' :
  fs.readFileSync(root + '/scaleform/grid-routing.as.inc', 'utf8');
const list = {
  skycrpgGrid: true, skycrpgCategoryFocus: false, disableInput:false, disableSelection:false,
  // Filtered/sorted entries deliberately differ from their displayed indices.
  entries:[{itemIndex:10},{itemIndex:40},{itemIndex:20},{itemIndex:70},{itemIndex:80},{itemIndex:90}],
  selectedIndex:10,
  getListEnumSize() { return this.entries.length; },
  getSelectedListEnumIndex() { return this.entries.findIndex(e => e.itemIndex === this.selectedIndex); },
  getListEnumEntry(i) { return this.entries[i]; },
  doSetSelectedIndex(i) { this.selectedIndex=i; },
  selectDefaultIndex() { if(this.entries.length) this.selectedIndex=this.entries[0].itemIndex; },
  skycrpgMove:move,
};
const panel = {itemList:list};
// Production routing is spliced before native CategoryList.handleInput.
const dispatch = new Function('details', route + `
  if (details.value === 'keyDown' && (details.navEquivalent === 'left' || details.navEquivalent === 'right')) {
    categories++; return true;
  }
  if (details.value === 'keyDown' && details.navEquivalent === 'enter' && this.itemList.selectedIndex !== -1) activations++;
  return false;
`);
// Expose counters to the dynamically created function without copying routing logic.
global.categories = 0; global.activations = 0;
const key = (nav,value='keyDown') => dispatch.call(panel,{navEquivalent:nav,value});
key('right'); assert.equal(list.selectedIndex,40); assert.equal(global.categories,0);
key('left'); assert.equal(list.selectedIndex,10); assert.equal(global.categories,0);
key('down'); assert.equal(list.selectedIndex,70);
key('up'); assert.equal(list.selectedIndex,10);
key('up'); assert.equal(list.selectedIndex,-1); assert.equal(list.skycrpgCategoryFocus,true);
key('right'); assert.equal(global.categories,1); assert.equal(list.selectedIndex,-1);
key('down'); assert.equal(list.selectedIndex,10); assert.equal(list.skycrpgCategoryFocus,false);
key('enter'); assert.equal(global.activations,1); // Exactly one native activation path.
key('right','keyUp'); assert.equal(list.selectedIndex,10);
list.disableInput = true; key('down'); assert.equal(list.selectedIndex,10);
list.disableInput = false; list.disableSelection = true; key('down'); assert.equal(list.selectedIndex,10);
list.disableSelection = false; list.skycrpgGrid = false; key('right'); assert.equal(global.categories,2);
assert.equal(global.activations,1);
console.log('PASS: grid before categories, native indices, category access, modal guards, one activation');
