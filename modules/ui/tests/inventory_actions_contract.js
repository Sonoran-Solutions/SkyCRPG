// Runs the actual player-menu hook with the pinned native method bodies.
// Mock GFx/native callbacks cannot establish Skyrim behavior or event order.
const fs = require('node:fs');
const assert = require('node:assert/strict');
const os = require('node:os');
const path = require('node:path');
const {spawnSync} = require('node:child_process');
const root = process.argv[2];
const staged = process.argv[3];
function methods(text, names) {
  const result = {};
  const pattern = /function\s+(\w+)\([^)]*\)\s*\{/g;
  let match;
  while ((match = pattern.exec(text))) {
    let end = pattern.lastIndex, depth = 1;
    while (depth) { if (text[end] === '{') depth++; else if (text[end] === '}') depth--; end++; }
    // Only the inherited handler is supplied by the harness; run the real
    // transformed InventoryMenu body and its capture ordering unchanged.
    if (!names || names.includes(match[1])) result[match[1]] = eval('(' + text.slice(match.index, end)
      .replace('super.onItemHighlightChange(event)', 'this.nativeHighlightChange(event)') + ')');
    pattern.lastIndex = end;
  }
  return result;
}
const transformed = fs.readFileSync(staged, 'utf8');
const hook = methods(transformed, ['skycrpgInventoryBridge','skycrpgInventoryCapture','skycrpgInventoryInput',
  'skycrpgInventoryAllow','skycrpgInventoryCancel','skycrpgInventoryViewChanged','onMouseDown','onMouseUp']);
const native = methods(transformed, ['handleInput','AttemptEquip','onItemSelect','DropItem','onQuantityMenuSelect','onItemHighlightChange']);
const lists = methods(fs.readFileSync(path.join(path.dirname(staged),'InventoryLists.as'),'utf8'),['InvalidateListData']);
let validations = 0, snapshots = 0, refreshes = 0, drops = 0, activates = 0, cancels = 0, views = 0;
let allowed = true;
const entry = {enabled:true, count:8, itemIndex:0};
global.skse = {plugins:{SkyCRPGUI:{
  getInventorySnapshot() { snapshots++; return {token:snapshots,selectedIndex:menu.inventoryLists.itemList.selectedEntry.itemIndex}; },
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

// Replay each fixture's history through the real portable ActionIntents policy.
// An optional include root permits the identical regression against a temporary
// copy of the old header. This still does not prove Skyrim's actual event order.
const scratch = fs.mkdtempSync(path.join(os.tmpdir(),'skycrpg-intents-'));
process.on('exit',()=>fs.rmSync(scratch,{recursive:true,force:true}));
const policy = path.join(scratch,'policy');
const compile = ['-std=c++20','-Wall','-Wextra','-Werror','-pedantic',
  '-I'+(process.argv[4] || path.join(root,'src')),
  path.join(root,'tests/inventory_action_intent_contract.cpp'),'-o',policy];
console.log('Portable policy adapter: g++ '+compile.join(' ')+' (timeout 600s)');
const build = spawnSync('g++',compile,{encoding:'utf8',timeout:600000});
assert.equal(build.status,0,build.stderr || String(build.error));
function fixture() {
  const state = {snapshots:0,drops:0,activates:0,refreshes:0,refusals:[],history:[],focused:true,updating:false,snapshotCalls:0};
  function step(command) {
    state.history.push(command);
    const history=path.join(scratch,'history');
    fs.writeFileSync(history,state.history.join('\n')+'\n');
    const result=spawnSync(policy,[history],{encoding:'utf8',timeout:10000});
    assert.equal(result.status,0,result.stderr || String(result.error));
    return result.stdout.trim().split('\n').at(-1);
  }
  const a = {enabled:true,count:8,itemIndex:7};
  const b = {enabled:true,count:8,itemIndex:12};
  const instance = Object.assign({}, hook, {
    _quantityMinCount:0,
    inventoryLists:{itemList:{selectedEntry:a,selectedIndex:0,entryList:[],InvalidateData(){}},
      categoryList:{selectedEntry:{flag:1},entryList:[],UpdateList(){}},
      dispatchEvent(event){if (event.type === 'itemHighlightChange') native.onItemHighlightChange.call(instance,event);},
      InvalidateListData(){state.refreshes++; lists.InvalidateListData.call(this);}},
    shouldProcessItemsListInput(){return true;}, updateBottomBar(){}, nativeHighlightChange(){}, skycrpgSyncStatus(){},
    confirmSelectedEntry(){return true;}, checkBook(){}, bFadedIn:true,
    itemCard:{ShowQuantityMenu(){}}, onQuantityMenuSelect:native.onQuantityMenuSelect,
  });
  global._root = {Menu_mc:instance};
  skse.plugins.SkyCRPGUI = {
    getInventorySnapshot() {
      state.snapshotCalls++;
      if (state.updating) return undefined; // Native rebuild guard, not a list observation.
      step('snapshot'); // Observe retires only a completed non-modal action.
      state.selectedIndex=instance.inventoryLists.itemList.selectedEntry.itemIndex;
      return {token:++state.snapshots,selectedIndex:instance.inventoryLists.itemList.selectedEntry.itemIndex};
    },
    inventoryInputEdge(key,down) {
      step(`edge ${key} ${Number(down)}`);
    },
    validateItemAction(token,action,amount,phase) {
      if (state.updating) return false;
      assert.equal(token,state.snapshots,'validator must receive the current bridge token');
      const focused=Number(state.focused && state.selectedIndex === instance.inventoryLists.itemList.selectedEntry.itemIndex);
      const result=step(phase === 'confirm' ? `confirm ${focused} ${amount}` :
        `begin ${token} ${Number(action === 'drop')} ${focused} ${amount} ${Number(phase === 'quantity')}`);
      if (result === 'allow') return true;
      state.refusals.push(result); step('cancel'); return false;
    },
    cancelInventoryAction(){step('cancel');}, inventoryViewChanged(){step('view');},
  };
  gfx.io.GameDelegate.call = name => { if (name === 'ItemDrop') state.drops++; if (name === 'ItemSelect') state.activates++; };
  function highlight(entry) {
    instance.inventoryLists.itemList.selectedEntry=entry;
    instance.inventoryLists.itemList.selectedIndex=entry.itemIndex;
    native.onItemHighlightChange.call(instance,{index:entry.itemIndex});
  }
  function edge(value) { native.handleInput.call(instance,{skseKeycode:46,value},[{handleInput(){return true;}}]); }
  return {state,instance,a,b,highlight,edge,drop(){native.DropItem.call(instance);},
    refresh(){lists.InvalidateListData.call(instance.inventoryLists);}};
}

// A native rebuild can re-enter highlight acquisition. The actual AS must
// retain undefined, then reacquire on the first explicit action after Update.
for (const callbackFirst of [false,true]) {
  const f=fixture();
  f.state.updating=true;
  f.highlight(f.a);
  assert.ok(f.state.snapshotCalls>0,'highlight must attempt acquisition during Update');
  assert.equal(f.instance._skycrpgInventoryToken,undefined);
  assert.equal(f.state.snapshots,0,'reentrant callbacks must not publish a token');
  assert.deepEqual(f.state.history,[],'reentrant snapshots must not Observe');
  assert.equal(f.state.refreshes,0);
  assert.equal(f.state.drops,0);
  f.state.updating=false;
  if (!callbackFirst) f.edge('keyDown');
  f.drop();
  assert.equal(f.state.drops,1,'undefined token must recapture and drop on the first press after Update');
  if (callbackFirst) f.edge('keyDown');
  f.edge('keyUp');
  assert.deepEqual(f.state.refusals,[]);
  assert.equal(f.state.refreshes,0);
}
console.log('PASS: reentrant rebuild snapshot is undefined; first post-update press recaptures in either callback/down order');

// The owner's sequence: DropItem BEFORE handleInput(keyDown), release, refresh,
// and repeat. A preceding successful action leaves the old policy consumed.
// Test both different rows and the same backing stack with its reduced count.
for (const sameStack of [false,true]) {
  if (process.argv[5] === 'same-stack' && !sameStack) continue;
  const f=fixture(); f.highlight(f.a); f.edge('keyDown'); f.drop(); f.edge('keyUp');
  assert.equal(f.state.drops,1,'preceding action must complete before testing next press');
  assert.deepEqual(f.state.refusals,[]);
  for (let press=0;press<2;press++) {
    const entry=sameStack ? {...f.a,count:7-press} : (press === 0 ? f.b : f.a);
    f.instance.inventoryLists.itemList.selectedEntry=entry;
    f.refresh();
    const before=f.state.drops;
    f.drop(); // native engine callback has not delivered this press's down yet
    assert.deepEqual(f.state.refusals,[],'engine callback before keyDown must not be refused');
    assert.equal(f.state.drops,before+1,`engine callback before keyDown: first press (${sameStack ? 'same stack' : 'different items'})`);
    f.edge('keyDown'); f.edge('keyUp');
    assert.deepEqual(f.state.refusals,[]);
  }
  assert.equal(f.state.drops,3); assert.equal(f.state.refreshes,0);
}
console.log('PASS: engine DropItem before handleInput keyDown twice, different items and refreshed same stack');

// AttemptEquip is delegated too; onItemSelect is the native mouse path. Both
// must allow each fresh press, including a callback before onMouseDown.
for (const action of ['equip','mouse']) for (const callbackFirst of [false,true]) {
  const f=fixture();
  for (const entry of [f.a,f.b]) {
    f.highlight(entry);
    if (!callbackFirst) {
      if (action === 'equip') f.edge('keyDown'); else f.instance.onMouseDown();
    }
    if (action === 'equip') native.AttemptEquip.call(f.instance,1,false);
    else native.onItemSelect.call(f.instance,{entry,keyboardOrMouse:1});
    assert.equal(f.state.activates,entry === f.a ? 1 : 2);
    if (action === 'equip') { if (callbackFirst) f.edge('keyDown'); f.edge('keyUp'); }
    else { if (callbackFirst) f.instance.onMouseDown(); f.instance.onMouseUp(); }
  }
  assert.deepEqual(f.state.refusals,[]);
}
console.log('PASS: engine AttemptEquip and mouse ItemSelect in either down/callback order on successive presses');

// No key event at all: mouse highlighting captures before the first drop.
{
  const f=fixture(); f.instance.skycrpgInventoryCapture();
  f.highlight(f.b); f.drop();
  assert.equal(f.state.drops,1,'mouse highlight must allow the first drop without a key event');
  assert.equal(f.state.refreshes,0);
}
console.log('PASS: mouse highlight without a key event allows first drop');

// A fresh press can precede mouse rollover. It captures A, then rollover B
// must replace that spent token. Both native actions should dispatch once.
{
  const f=fixture(); f.highlight(f.a); f.edge('keyDown'); f.drop(); f.edge('keyUp');
  f.edge('keyDown'); f.highlight(f.b); f.drop();
  assert.equal(f.state.drops,2,'mouse A/action/B/action must both dispatch');
  assert.equal(f.state.refreshes,0);
}
console.log('PASS: mouse A/action/B/action');

// Engine DropItem need not wait for handleInput or the highlight notification.
// Keep a spent A token through a fresh input edge, change native selection to B,
// then deliver DropItem before onItemHighlightChange.
{
  const f=fixture(); f.highlight(f.a); f.edge('keyDown'); f.drop(); f.edge('keyUp');
  f.edge('keyDown');
  f.instance.inventoryLists.itemList.selectedEntry=f.b;
  f.instance.inventoryLists.itemList.selectedIndex=f.b.itemIndex;
  f.drop();
  assert.equal(f.state.drops,2,'mouse selection before highlight callback must allow first drop');
  assert.equal(f.state.refreshes,0);
}
console.log('PASS: mouse drop before highlight callback');

// Inherited highlight processing can also make a native callback reentrant.
{
  const f=fixture(); f.highlight(f.a); f.edge('keyDown'); f.drop(); f.edge('keyUp');
  f.edge('keyDown'); f.instance.nativeHighlightChange=()=>f.drop(); f.highlight(f.b);
  assert.equal(f.state.drops,2,'capture must precede inherited highlight callbacks');
}

// Repeated highlights on the same backing entry must not mint a token. Neither
// a held repeat nor release/repress can reuse the consumed entry's intent.
for (const repeat of ['keyHold','keyDown','freshPress']) {
  const f=fixture(); f.highlight(f.a); f.edge('keyDown'); f.drop();
  const token=f.instance._skycrpgInventoryToken, count=f.state.snapshots;
  f.highlight({...f.a}); // native backing index, not AS object identity
  if (repeat === 'freshPress') { f.edge('keyUp'); f.edge('keyDown'); } else f.edge(repeat);
  assert.equal(f.instance._skycrpgInventoryToken,token,'same entry must retain its spent token');
  assert.equal(f.state.snapshots,count,'same entry must not mint a token');
  f.drop(); f.drop();
  assert.equal(f.state.drops,1);
  assert.deepEqual(f.state.refusals,['duplicate-input','duplicate-input']);
  assert.equal(f.instance.inventoryLists.itemList.selectedIndex,-1);
}
// Even a new token for B must not bypass _consumed while the action key is held.
{
  const f=fixture(); f.highlight(f.a); f.edge('keyDown'); f.drop(); f.highlight(f.b); f.drop();
  assert.equal(f.state.drops,1); assert.deepEqual(f.state.refusals,['duplicate-input']);
}
console.log('PASS: held repeats, same-entry repress and held highlight changes retain duplicate protection');

// View refresh may mint a token while a key is held, but must not release it.
// After a real refusal, neither the late down nor refresh retries the action.
for (const callbackFirst of [false,true]) {
  const f=fixture(); f.highlight(f.a); f.state.focused=false;
  if (!callbackFirst) f.edge('keyDown');
  f.drop(); assert.equal(f.state.drops,0);
  f.state.focused=true;
  if (callbackFirst) f.edge('keyDown');
  f.refresh(); f.drop();
  assert.equal(f.state.drops,0); assert.deepEqual(f.state.refusals,['focus','duplicate-input']);
  f.edge('keyUp'); f.refresh();
  assert.equal(f.state.drops,0,'release and refresh must not automatically retry');
  f.drop(); assert.equal(f.state.drops,1,'a new explicit press may succeed');
}
{
  const f=fixture(); f.highlight(f.a); f.edge('keyDown'); f.drop(); f.refresh();
  f.edge('keyHold'); f.edge('keyDown'); f.drop();
  assert.equal(f.state.drops,1); assert.deepEqual(f.state.refusals,['duplicate-input']);
}
console.log('PASS: refusal is spent in both orders; refresh/held repeat cannot retry');

for (const close of ['confirm','cancel']) {
  const f=fixture(); f.instance._quantityMinCount=5;
  f.highlight(f.a); f.edge('keyDown'); f.drop();
  const token=f.instance._skycrpgInventoryToken, count=f.state.snapshots;
  f.highlight(f.b); f.highlight(f.a); f.edge('keyUp'); f.edge('keyDown');
  assert.equal(f.instance._skycrpgInventoryToken,token);
  assert.equal(f.state.snapshots,count,'modal highlights must not call getInventorySnapshot');
  if (close === 'confirm') f.instance.onQuantityMenuSelect({amount:3});
  else f.instance.skycrpgInventoryCancel();
  assert.equal(f.state.drops,close === 'confirm' ? 1 : 0);
  assert.equal(f.instance._skycrpgInventoryQuantity,false);
  f.highlight(f.b);
  assert.ok(f.state.snapshots>count,'highlight capture resumes after modal close');
}
console.log('PASS: quantity highlights freeze the token until confirm/cancel');

// A real quantity confirmation is a separate press after the initiating up.
// It consumes that press even when its down arrives after ItemDrop dispatch.
for (const callbackFirst of [false,true]) {
  const f=fixture(); f.instance._quantityMinCount=5;
  f.highlight(f.a); f.drop(); f.edge('keyDown'); f.edge('keyUp');
  if (!callbackFirst) f.edge('keyDown');
  f.instance.onQuantityMenuSelect({amount:3}); assert.equal(f.state.drops,1);
  if (callbackFirst) f.edge('keyDown');
  f.instance.onQuantityMenuSelect({amount:3}); assert.equal(f.state.drops,1);
  assert.deepEqual(f.state.refusals,['pending-intent']);
  f.edge('keyUp');
}
console.log('PASS: quantity confirm dispatches ItemDrop once in either down/callback order');
