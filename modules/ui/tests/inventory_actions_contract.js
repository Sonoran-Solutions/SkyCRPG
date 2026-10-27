// Runs the actual player-menu hook with the pinned native method bodies.
// Mock GFx/native callbacks cannot establish Skyrim behavior or event order.
const fs = require('node:fs');
const assert = require('node:assert/strict');
const root = process.argv[2];
const staged = process.argv[3];
function methods(text, names) {
  const result = {};
  const pattern = /function\s+(\w+)\([^)]*\)\s*\{/g;
  let match;
  while ((match = pattern.exec(text))) {
    let end = pattern.lastIndex, depth = 1;
    while (depth) { if (text[end] === '{') depth++; else if (text[end] === '}') depth--; end++; }
    if (!names || names.includes(match[1])) result[match[1]] = eval('(' + text.slice(match.index, end) + ')');
    pattern.lastIndex = end;
  }
  return result;
}
const hook = methods(fs.readFileSync(root + '/scaleform/inventory-actions.as.inc', 'utf8'));
const native = methods(fs.readFileSync(staged, 'utf8'), ['AttemptEquip','onItemSelect','DropItem','onQuantityMenuSelect']);
let validations = 0, snapshots = 0, refreshes = 0, drops = 0, activates = 0, cancels = 0, views = 0;
let allowed = true;
const entry = {enabled:true, count:8, itemIndex:0};
global.skse = {plugins:{SkyCRPGUI:{
  getInventorySnapshot() { snapshots++; return {token:snapshots}; },
  validateItemAction(token, action, amount, phase) { validations++; assert.ok(token > 0); return allowed; },
  inventoryInputEdge(){}, cancelInventoryAction(){cancels++;}, inventoryViewChanged(){views++;},
}}};
global.gfx = {io:{GameDelegate:{call(name){ if(name === 'ItemSelect') activates++; if(name === 'ItemDrop') drops++; }}}};
const menu = Object.assign({}, hook, {
  _quantityMinCount:5,
  inventoryLists:{itemList:{selectedEntry:entry,selectedIndex:0},InvalidateListData(){refreshes++;}},
  shouldProcessItemsListInput(){return true;}, confirmSelectedEntry(){return true;}, checkBook(){},
  itemCard:{ShowQuantityMenu(){menu.shown=true;}},
  onQuantityMenuSelect:native.onQuantityMenuSelect,
});
menu.skycrpgInventoryInput({skseKeycode:1,value:'keyDown'});
assert.equal(snapshots,1);
menu.skycrpgInventoryInput({skseKeycode:1,value:'keyHold'});
menu.skycrpgInventoryInput({skseKeycode:1,value:'keyDown'});
assert.equal(snapshots,1);
native.AttemptEquip.call(menu,1,false); assert.equal(validations,1); assert.equal(activates,1);
native.onItemSelect.call(menu,{entry,keyboardOrMouse:1}); assert.equal(validations,2); assert.equal(activates,2);
native.onItemSelect.call(menu,{entry:{...entry},keyboardOrMouse:1}); assert.equal(validations,2); assert.equal(activates,2);
allowed=false;
native.AttemptEquip.call(menu,1,false); assert.equal(validations,3); assert.equal(activates,2); assert.equal(refreshes,1);
allowed=true;
menu.skycrpgInventoryInput({skseKeycode:1,value:'keyUp'});
menu.skycrpgInventoryInput({skseKeycode:1,value:'keyDown'});
const captured=menu._skycrpgInventoryToken;
native.DropItem.call(menu); assert.equal(validations,4); assert.equal(menu.shown,true); assert.equal(drops,0);
menu.skycrpgInventoryInput({skseKeycode:2,value:'keyDown'}); assert.equal(menu._skycrpgInventoryToken,captured);
menu.onQuantityMenuSelect({amount:3}); assert.equal(validations,5); assert.equal(drops,1);
allowed=false;
menu.onQuantityMenuSelect({amount:3}); assert.equal(validations,6); assert.equal(drops,1);
menu.skycrpgInventoryCancel(); assert.equal(cancels,1); assert.equal(menu._skycrpgInventoryQuantity,false);
menu.skycrpgInventoryViewChanged(); assert.equal(views,1);
// Missing and old bridge retain both native paths and native quantity handling.
for (const bridge of [undefined,{}, {getInventorySnapshot(){}}, {getInventorySnapshot(){}, validateItemAction(){throw Error('partial bridge used');}}]) {
  skse.plugins.SkyCRPGUI=bridge;
  native.AttemptEquip.call(menu,1,false);
  native.onItemSelect.call(menu,{entry,keyboardOrMouse:1});
  native.DropItem.call(menu); menu.onQuantityMenuSelect({amount:1});
}
assert.equal(validations,6); assert.equal(activates,10); assert.equal(drops,5);
assert.ok(fs.readFileSync(staged,'utf8').includes('event.canceled == true'));
console.log('PASS: actual player native activation/drop bodies, one validator per dispatch, stale refusal, quantity token, absent/old bridge fallback');
