// Actual transformed AS under movie/native-list stubs, not a GFx/game test.
const fs = require('node:fs');
const assert = require('node:assert/strict');
const {createHash} = require('node:crypto');
const {spawnSync} = require('node:child_process');
const root = process.argv[2], provider = process.argv[3];
// Purpose: exercise the production builder's sources. Completion: exit 0 + JSON;
// owner DYL-164; bounded 600s child; no files or persistent children are created.
const prepare = spawnSync('python3', ['-I','-c', `
import importlib.util,json,pathlib,sys
r=pathlib.Path(sys.argv[1]);p=pathlib.Path(sys.argv[2])/'source/actionscript'
s=importlib.util.spec_from_file_location('builder',r/'tools/build_scaleform.py');b=importlib.util.module_from_spec(s);s.loader.exec_module(b)
print(json.dumps({n:b.transform(n,(p/n).read_text()) for n in b.SOURCES}))
`,root,provider], {encoding:'utf8',timeout:600000});
assert.equal(prepare.status,0,prepare.stderr);
const staged = JSON.parse(prepare.stdout);
function method(source,name) {
  const match = new RegExp('function '+name+'\\s*\\([^)]*\\)\\s*\\{').exec(source);
  assert.ok(match,name);
  const start=match.index; let end=source.indexOf('{',start)+1, depth=1;
  while(depth && end<source.length) { if(source[end]==='{')depth++; if(source[end]==='}')depth--; end++; }
  const text=source.slice(start,end).replace(/:\s*(Object|Number|String|Boolean|MovieClip|ListState|InputDetails|Array)\b(?=\s*[,;)])/g,'')
    .replace(/function (get|set) /,'function ')
    .replace(/super\.(\w+)\(/g,'this.native_$1(');
  return eval('('+text+')');
}
// AS2 intervals repeat until explicitly cleared; callbacks can cancel another
// interval in the same tick. Keep leaked IDs active rather than hiding them.
const intervals=new Map();
let nextInterval=1;
global.setInterval=(target,name,delay)=>{
  const id=nextInterval++;intervals.set(id,{target,name,delay});return id;
};
global.clearInterval=id=>intervals.delete(id);
function tick() {
  for(const [id,{target,name}] of [...intervals]) if(intervals.has(id)) target[name]();
}
function drain(label) {
  for(let i=0;i<5 && intervals.size;i++) tick();
  assert.equal(intervals.size,0,label+': zero active intervals after callbacks');
}
class MovieClip {
  constructor() { this._visible=true; this._totalframes=1; this._currentframe=1; }
  clear() {} lineStyle() {} beginFill() {} endFill() {} moveTo() {} lineTo() {} curveTo() {}
  createEmptyMovieClip(key) { return this[key]=new MovieClip(); }
  createTextField(key,depth,x,y,width,height) { this[key]=new TextField(width,height); }
  gotoAndStop(label) { this._currentframe=label===1 ? 1 : (label==='known' ? 2 : this._currentframe); }
  getNextHighestDepth(){this.depth=(this.depth||1000)+1;return this.depth;}
  swapDepths(depth){this.layer=depth;}
  getBounds(){return {xMin:-15,yMin:50,xMax:485,yMax:650};}
  hitTest(){return false;}
}
class TextField {
  constructor(width,height) { this._width=width; this._height=height; this.text=''; }
  setTextFormat(){} setNewTextFormat(){}
  get textHeight(){ return Math.ceil(this.text.length / Math.floor(this._width/7))*15; }
}
global.MovieClip=MovieClip; global.TextField=TextField; global.TextFormat=function(){};
global.Shared={GlobalFunc:{IsKeyPressed:d=>d.value==='keyDown'||d.value==='keyHold'}};
global.gfx={ui:{NavigationCode:{UP:'up',DOWN:'down',LEFT:'left',RIGHT:'right'}},
  io:{GameDelegate:{call(name){if(name==='ItemSelect') dispatches++;}}}};
global.skyui={components:{list:{BasicList:{SELECT_MOUSE:0,SELECT_KEYBOARD:1}}}};
global._root={_xmouse:0,_ymouse:0};
const source=staged['Common/skyui/components/list/ScrollingList.as'];
const list={skycrpgGrid:true,_bSuspended:false,disableInput:false,disableSelection:false,
  background:{_x:0,_y:0},leftBorder:0,topBorder:0,entryHeight:28,scrollDelta:3,
  entries:[],_scrollPosition:0,_selectedIndex:-1,_maxListIndex:18,_listIndex:0,
  isMouseDrivenNav:false, dispatchEvent(){}, updateScrollbar(){}, paints:0, largest:0};
for(const name of ['skycrpgPaintCell','skycrpgCellText','skycrpgStatus','skycrpgTypeIcon','skycrpgMove',
  'skycrpgUpdateScrollPosition','skycrpgCalculateMaxScrollPosition','UpdateList','doSetSelectedIndex']) list[name]=method(source,name);
list.updateScrollPosition=list.skycrpgUpdateScrollPosition;
list.calculateMaxScrollPosition=list.skycrpgCalculateMaxScrollPosition;
Object.defineProperty(list,'scrollPosition',{get(){return this._scrollPosition;},set:method(source,'set scrollPosition')});
Object.defineProperty(list,'selectedEntry',{get(){return this.entries.find(e=>e.itemIndex===this._selectedIndex)||{};}});
list.getListEnumSize=function(){return this.entries.length;};
list.getListEnumEntry=function(i){return this.entries[i];};
list.getListEnumIndex=function(i){const n=this.entries.findIndex(e=>e.itemIndex===i);return n<0?undefined:n;};
list.getSelectedListEnumIndex=function(){return this.getListEnumIndex(this._selectedIndex);};
const manager=fs.readFileSync(provider+'/source/actionscript/Common/skyui/components/list/EntryClipManager.as','utf8');
const pool={_clipPool:[],growPool(n){while(this._clipPool.length<n){const clip=new MovieClip();clip.itemIcon=new MovieClip();clip.setEntry=function(entry){list.paints++;list.skycrpgPaintCell(this,entry);};this._clipPool.push(clip);}}};
Object.defineProperty(pool,'clipCount',{set:method(manager,'set clipCount')});
list.setClipCount=function(n){this.largest=Math.max(this.largest,n);pool.clipCount=n;};
list.getClipByIndex=i=>pool._clipPool[i];
const cell=new MovieClip(); cell.itemIndex=91; const listener=()=>{}; cell.onPress=listener;
cell.itemIcon=new MovieClip();cell.itemIcon._totalframes=3;
const long='Extremely long custom name <&> '.repeat(1000);
const native={itemIndex:91,text:long,count:123,favorite:true,isStolen:true,isQuestItem:true,equipState:1,formType:41,iconLabel:'known'};
list.skycrpgPaintCell(cell,native);
assert.equal(cell.itemIndex,91);assert.equal(cell.onPress,listener);assert.equal(native.text,long);
assert.ok(cell.skycrpgName.text.length<=32);assert.equal(cell.skycrpgCount.text,'x123');
assert.equal(cell.skycrpgFlags.text,'E F S Q');assert.equal(cell.itemIcon._visible,true);
list.skycrpgPaintCell(cell,{itemIndex:92,text:'No icon',count:0,formType:26,iconLabel:'missing'});
assert.equal(cell.itemIcon._visible,false); assert.equal(cell.skycrpgFallback._visible,true);
assert.equal(cell.skycrpgCount.text,'x0'); assert.equal(cell.skycrpgFlags.text,'E? F? S? Q?');
list.skycrpgObservation=[{itemIndex:92,count:1,available:true,quest:true,worn:false,wornLeft:false,favorite:false,stolen:false}];
assert.equal(list.skycrpgStatus({itemIndex:92,count:1},false),'Q');
assert.equal(list.skycrpgStatus({itemIndex:92,count:2},false),'E? F? S? Q?');
list.skycrpgObservation[0].available=false;
assert.equal(list.skycrpgStatus({itemIndex:92,count:1},false),'E? F? S? Q?');
list.skycrpgObservation=undefined;
for(const n of [0,1,17,18,19,20,21,499,500]) {
  // Deliberately reversed mapping; enum position is never the backing itemIndex.
  list.entries=Array.from({length:n},(_,i)=>({itemIndex:n-i-1,text:'Entry '+(i+1),count:1,formType:32}));
  list._scrollPosition=0;list._selectedIndex=-1;list.calculateMaxScrollPosition();list.UpdateList();
  assert.equal(list._maxScrollPosition,Math.max(0,Math.ceil(n/3)*3-18));
  list.scrollPosition=1;assert.equal(list.scrollPosition,0);
  for(let i=0;i<n;i++) {
    list.paints=0;list.skycrpgMove({navEquivalent:'right',value:'keyDown'});
    assert.equal(list._selectedIndex,n-i-1);
    assert.equal(list.scrollPosition%3,0);
    assert.ok(list.paints<=20); // two selection paints + at most 18 window paints
    const visible=pool._clipPool.filter(c=>c._visible);
    assert.ok(visible.length<=18);
    assert.ok(visible.some(c=>c.itemIndex===list._selectedIndex));
    assert.equal(visible[0]._x,0);assert.equal(visible[0]._y,0);
  }
  if(n) assert.equal(list.selectedEntry.text,'Entry '+n);
  assert.ok(pool._clipPool.length<=18);
  // Large -> tiny filter cannot leave old visible clips or stale indices.
  list.entries=[];list.calculateMaxScrollPosition();list.UpdateList();
  assert.equal(pool._clipPool.filter(c=>c._visible).length,0);
}
assert.equal(list.largest,18);
// Execute all four unmodified timer bodies from the actual pinned provider.
// BasicList is test input only, never a compiled SkyCRPG source.
const basic=fs.readFileSync(provider+'/source/actionscript/Common/skyui/components/list/BasicList.as','utf8');
assert.equal(createHash('sha256').update(basic).digest('hex'),
  '99012374aeece667f0a56fdc74119df9a6c0c42ec53c21333b28111de6eea7a2');
for(const name of ['requestUpdate','requestInvalidate','commitUpdate','commitInvalidate']) list[name]=method(basic,name);
list.InvalidateData=function(){this.calculateMaxScrollPosition();this.UpdateList();};
const menuSource=staged['ItemMenus/InventoryMenu.as'];
function presentationMenu(itemList=list) {
  const menu={inventoryLists:{itemList}};
  for(const name of ['skycrpgInventoryPresent','skycrpgRepaintInventory','skycrpgPresentationClose','onUnload']) {
    // An old-source negative control must reach the leak assertion, even though
    // that revision has no SkyCRPG repaint callback.
    if(menuSource.includes('function '+name+'(')) menu[name]=method(menuSource,name);
  }
  return menu;
}
for(const n of [2,3,50]) for(const native of ['none','before','after'])
  for(const invalidation of ['none','before','after']) {
    const label=`${n} snapshots, native update ${native}, invalidate ${invalidation}`;
    const repaintMenu=presentationMenu();
    list.entries=Array.from({length:18},(_,i)=>({itemIndex:i,text:'Timer row '+i,count:1}));
    list._scrollPosition=0;list._selectedIndex=-1;list.calculateMaxScrollPosition();list.paints=0;
    if(invalidation==='before') list.requestInvalidate();
    if(native==='before') list.requestUpdate();
    let latest;
    for(let i=0;i<n;i++) repaintMenu.skycrpgInventoryPresent({entries:latest=[{itemIndex:i,count:i+1}]});
    if(native==='after') list.requestUpdate();
    if(invalidation==='after') list.requestInvalidate();
    assert.equal(list.skycrpgObservation,latest,label+': latest snapshot wins');
    drain(label);
    assert.ok(list.paints>=18 && list.paints<=36,label+': at least one, at most two visible-window paints');
    assert.equal(repaintMenu._skycrpgRepaintTimer,undefined);
    assert.equal(list._updateRequestID,undefined);assert.equal(list._invalidateRequestID,undefined);
    const paints=list.paints;tick();tick();assert.equal(list.paints,paints,label+': stays idle');
  }
// Pending native commits are left untouched; our one-shot must skip its paint.
for(const request of ['requestUpdate','requestInvalidate']) {
  const repaintMenu=presentationMenu();list.paints=0;
  repaintMenu.skycrpgInventoryPresent(undefined);list[request]();
  assert.equal(list.skycrpgObservation,undefined);
  tick();assert.equal(list.paints,18,request+': exactly the native commit paints');
  drain(request);
}
// Each menu owns its own one-shot. Close and unload cancel only that instance.
for(const close of ['skycrpgPresentationClose','onUnload']) {
  const first=presentationMenu(),second=presentationMenu({...list,paints:0});
  first.skycrpgInventoryPresent({entries:[]});second.skycrpgInventoryPresent({entries:[]});
  assert.equal(intervals.size,2);
  first[close]();assert.equal(first._skycrpgRepaintTimer,undefined);
  assert.equal(list.skycrpgObservation,undefined);assert.equal(intervals.size,1);
  second[close]();drain(close);
}
const closingMenu=presentationMenu();list.paints=0;
closingMenu.skycrpgInventoryPresent({entries:[]});closingMenu._bMenuClosing=true;
drain('closing callback');assert.equal(list.paints,0);
closingMenu.skycrpgInventoryPresent({entries:[]});assert.equal(intervals.size,0);
console.log('PASS: pinned BasicList timers; 27 rapid-snapshot/native-update/invalidate interleavings, bounded repaint, idle after callbacks, per-menu close/unload cleanup');
// Selection still reaches the actual gated dispatch; painting/navigation cannot.
let dispatches=0, validations=0;
list.entries=[{itemIndex:19,text:'Sword',count:1},{itemIndex:7,text:'Potion',count:2}];
list.calculateMaxScrollPosition();list.UpdateList();
list._selectedIndex=19;list.skycrpgMove({navEquivalent:'right',value:'keyDown'});
assert.equal(list._selectedIndex,7);assert.equal(dispatches,0);
const menu={inventoryLists:{itemList:list},shouldProcessItemsListInput(){return true;},confirmSelectedEntry(){return true;},checkBook(){}};
for(const name of ['skycrpgInventoryBridge','skycrpgInventoryCapture','skycrpgInventoryAllow','skycrpgInventoryPresent','AttemptEquip']) menu[name]=method(menuSource,name);
if(menuSource.includes('function skycrpgRepaintInventory(')) menu.skycrpgRepaintInventory=method(menuSource,'skycrpgRepaintInventory');
global.skse={plugins:{SkyCRPGUI:{validateItemAction(){validations++;return true;},getInventorySnapshot(){return {token:3,selectedIndex:7};},inventoryInputEdge(){},cancelInventoryAction(){},inventoryViewChanged(){}}}};
menu.AttemptEquip(1,false);assert.equal(validations,1);assert.equal(dispatches,1);
skse.plugins.SkyCRPGUI.validateItemAction=()=>false;list.InvalidateListData=()=>{};menu.inventoryLists.InvalidateListData=()=>{};
menu.AttemptEquip(1,false);assert.equal(dispatches,1);
drain('gated actions');
// Actual equipment drawing: all eight slots clear previous data on absence.
const equipmentMenu={_skycrpgPanel:new MovieClip(), inventoryLists:{itemList:list}};
equipmentMenu.skycrpgDrawEquipment=method(menuSource,'skycrpgDrawEquipment');
equipmentMenu.skycrpgDrawEquipment([{known:true,empty:true},{known:true,empty:false,name:'Iron Armor',formType:26}]);
assert.equal(equipmentMenu._skycrpgPanel.slot0.name.text,'Empty');
assert.equal(equipmentMenu._skycrpgPanel.slot1.name.text,'Iron Armor');
assert.equal(equipmentMenu._skycrpgPanel.slot7.name.text,'Unknown');
equipmentMenu.skycrpgDrawEquipment(undefined);
assert.equal(equipmentMenu._skycrpgPanel.slot1.name.text,'Unknown');
// Refresh coalesces a burst to one 100ms task, never a per-frame actor read.
for(const name of ['skycrpgRequestEquipment','skycrpgRefreshEquipment','skycrpgPresentationClose']) equipmentMenu[name]=method(menuSource,name);
let equipmentDraws=0;
const drawEquipment=equipmentMenu.skycrpgDrawEquipment;
equipmentMenu.skycrpgDrawEquipment=function(slots){equipmentDraws++;drawEquipment.call(this,slots);};
for(let i=0;i<500;i++) equipmentMenu.skycrpgRequestEquipment();
assert.equal(equipmentDraws,1);
assert.equal(intervals.size,1);assert.equal(intervals.get(equipmentMenu._skycrpgEquipmentTimer).delay,100);
drain('equipment refresh');assert.equal(equipmentDraws,2);
assert.equal(equipmentMenu._skycrpgEquipmentTimer,undefined);
assert.equal(equipmentMenu._skycrpgPanel.slot1.name.text,'Unknown');
equipmentMenu.skycrpgRequestEquipment();equipmentMenu.skycrpgPresentationClose();drain('equipment close');
assert.equal(equipmentMenu._skycrpgEquipmentTimer,undefined);
const draws=equipmentDraws;tick();assert.equal(equipmentDraws,draws);
console.log('PASS: native paint/fallbacks; 0/1/17/18/19/20/21/499/500 rows, 18-clip pool, row alignment, selection gates, empty/unknown equipment and bounded refresh');

// Layout executes the player-list helper and menu fitting against representative
// native art bounds. Pixels/fonts/timeline behavior still need game captures.
const panelLayout=method(staged['ItemMenus/InventoryLists.as'],'skycrpgLayout');
for(const rect of [{x:0,y:0,width:1280,height:720},{x:0,y:0,width:1920,height:1080},
  {x:50,y:30,width:1024,height:600}]) {
  global.Stage={visibleRect:rect};
  function box(x,y,w,h,parent) { return {_x:x,_y:y,_width:w,_height:h,getBounds() {return {xMin:(parent?parent._x:0)+this._x,yMin:(parent?parent._y:0)+this._y,xMax:(parent?parent._x:0)+this._x+this._width,yMax:(parent?parent._y:0)+this._y+this._height};}}; }
  const layoutList={skycrpgGrid:true,scrollbar:{}};
  layoutList.header=box(0,-30,516,20,layoutList); layoutList.background=box(0,0,516,444,layoutList);
  const lists={_x:0,_y:0,itemList:layoutList,panelContainer:{ListBackground:{}},categoryList:box(0,12,510,44),
    categoryLabel:box(8,67,145,22),searchWidget:box(0,0,140,20),columnSelectButton:box(0,0,18,20),skycrpgLayout:panelLayout};
  const holder=new MovieClip();
  const layoutMenu=new MovieClip();
  Object.assign(layoutMenu,{inventoryLists:lists,itemCard:{_parent:holder},mouseRotationRect:{Lock(){}},bottomBar:{getBounds(){return {yMin:rect.y+rect.height-50};}},
    skycrpgDrawEquipment(){},skycrpgPositionCard:method(menuSource,'skycrpgPositionCard'),skycrpgLayout:method(menuSource,'skycrpgLayout')});
  layoutMenu.skycrpgLayout();
  const scale=layoutMenu._skycrpgScale;
  assert.ok(layoutMenu._skycrpgPanel._x+264*scale<=lists._x);
  assert.ok(lists._x+542*scale<=layoutMenu._skycrpgDetailPanel._x);
  assert.ok(lists._y+(layoutList._y+444)*scale<=rect.y+rect.height);
  assert.ok(lists.searchWidget._x+lists.searchWidget._width<=520);
  assert.equal(layoutList.listHeight,444);assert.ok(layoutList.header.getBounds().yMin>=lists.searchWidget.getBounds().yMax);
  assert.ok(holder._x-15*holder._xscale/100>=layoutMenu._skycrpgDetailPanel._x-0.001);
  assert.ok(holder._x+485*holder._xscale/100<=rect.x+rect.width);
  layoutMenu._skycrpgModal=true;layoutMenu.skycrpgPositionCard();
  assert.equal(layoutMenu._skycrpgModalShade._visible,true);
  assert.ok(holder.layer>layoutMenu._skycrpgModalShade.layer);
  assert.ok(holder._y+650*holder._yscale/100<=rect.y+rect.height);
  layoutMenu._skycrpgModal=false;layoutMenu.skycrpgPositionCard();
  assert.equal(layoutMenu._skycrpgModalShade._visible,false);
  // Calling the added shared-list helper in a native menu has no effect.
  layoutList.skycrpgGrid=false;layoutList._x=77;panelLayout.call(lists);assert.equal(layoutList._x,77);
}
console.log('PASS: 720p/1080p/narrow geometry, visible native controls, non-player no-op and native modal above backdrop');
