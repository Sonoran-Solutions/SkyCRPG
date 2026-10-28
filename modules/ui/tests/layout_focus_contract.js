// Executes compiled-source transforms with movie stubs. No GFx/font/game claim.
const fs = require('node:fs');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const {spawnSync} = require('node:child_process');
const root = process.argv[2], provider = process.argv[3];
// Child purpose: transform pinned sources; owner DYL-168; timeout 600s;
// completion exit 0/JSON; synchronous child reaped, no output files.
const prepare = spawnSync('python3', ['-I', '-c', `
import importlib.util,json,pathlib,sys
r=pathlib.Path(sys.argv[1]);p=pathlib.Path(sys.argv[2])/'source/actionscript'
s=importlib.util.spec_from_file_location('builder',r/'tools/build_scaleform.py');b=importlib.util.module_from_spec(s);s.loader.exec_module(b)
print(json.dumps({n:b.transform(n,(p/n).read_text()) for n in b.SOURCES}))
`, root, provider], {encoding:'utf8', timeout:600000});
assert.equal(prepare.status, 0, prepare.stderr);
const staged = JSON.parse(prepare.stdout);
function method(source, name) {
  const start = source.indexOf('function ' + name + '(');
  assert.ok(start >= 0, name);
  let end = source.indexOf('{', start) + 1, depth = 1;
  while (depth) { if (source[end] === '{') depth++; if (source[end] === '}') depth--; end++; }
  return eval('(' + source.slice(start, end).replace(/:\s*(Object|Number|String|Boolean|MovieClip|Array)\b(?=\s*[,;)])/g,'') + ')');
}
class Clip {
  constructor() { this._visible = true; this.points = []; }
  createEmptyMovieClip(name) { return this[name] = new Clip(); }
  createTextField(name, depth, x, y, width, height) {
    this[name] = {_x:x,_y:y,_width:width,_height:height,setNewTextFormat(){},setTextFormat(){},textHeight:15};
  }
  clear() { this.points = []; } lineStyle() {} beginFill() {} endFill() {}
  moveTo(x,y) { this.points.push([x,y]); } lineTo(x,y) { this.points.push([x,y]); } curveTo() {}
  getNextHighestDepth() { this.depth = (this.depth || 20000) + 1; return this.depth; } swapDepths(depth) { this.layer = depth; }
  getBounds() { return {xMin:-15,yMin:50,xMax:485,yMax:650}; }
}
global.TextFormat = function() {};
global.gfx = {ui:{NavigationCode:{UP:'up',DOWN:'down',LEFT:'left',RIGHT:'right',ENTER:'enter',TAB:'tab',SHIFT_TAB:'shiftTab'}}};
global.Shared = {GlobalFunc:{IsKeyPressed:d=>d.value==='keyDown'||d.value==='keyHold'}};
const listsSource = staged['ItemMenus/InventoryLists.as'];
const gridSource = staged['Common/skyui/components/list/ScrollingList.as'];
const menuSource = staged['ItemMenus/InventoryMenu.as'];
function listsFixture() {
  const list = {skycrpgGrid:true,selectedIndex:10,skycrpgCategoryFocus:false,
    entries:[{itemIndex:10},{itemIndex:40},{itemIndex:20},{itemIndex:70}],
    getListEnumSize(){return this.entries.length;},
    getSelectedListEnumIndex(){return this.entries.findIndex(e=>e.itemIndex===this.selectedIndex);},
    getListEnumEntry(i){return this.entries[i];},
    doSetSelectedIndex(i){this.selectedIndex=i;},selectDefaultIndex(){this.selectedIndex=this.entries[0].itemIndex;},
    header:{},background:{},scrollbar:{}};
  for (const name of ['skycrpgMove','skycrpgCellText']) list[name] = method(gridSource,name);
  const lists = {_currentState:1,_platform:1,_sortOrderKey:272,_searchKey:57,
    itemList:list,panelContainer:new Clip(),categoryList:{handleInput(){return false;}},
    categoryLabel:{},searchWidget:{},columnSelectButton:{},_nameFilter:{filterText:''}};
  lists.panelContainer.ListBackground = {};
  for (const name of ['skycrpgLayout','skycrpgDrawFocus','skycrpgSearchInput','skycrpgDrawSearch',
    'skycrpgEndSearch','skycrpgSearchLabel','handleInput','onSearchInputStart','onSearchInputEnd']) lists[name] = method(listsSource,name);
  lists.skycrpgLayout();
  return lists;
}
global.InventoryLists = {SHOW_PANEL:1};
const event = (nav, value='keyDown', skseKeycode=0) => ({navEquivalent:nav,value,skseKeycode});
// The actual InventoryLists handler must consume pad input before native sort,
// search and the focused list; accidentally arming a native sort timer fails.
global.setInterval = () => { throw Error('native sort timer ran behind search'); };
let nativeCalls = 0;
const path = () => [{handleInput(){nativeCalls++; return false;}}];
for (const platform of [0,1]) {
  const lists = listsFixture(); lists._platform=platform;
  const key = (nav,value='keyDown',code=0) => lists.handleInput(event(nav,value,code),path());
  key('up'); assert.equal(lists.itemList.skycrpgCategoryFocus,true);
  assert.ok(lists._skycrpgFocus.points.some(([x,y])=>x===0&&y===0));
  key('down'); assert.equal(lists.itemList.selectedIndex,10);
  assert.equal(lists.itemList.skycrpgCategoryFocus,false);
  assert.ok(lists._skycrpgFocus.points.some(([x,y])=>x===0&&y===120));
  key('right'); assert.equal(lists.itemList.selectedIndex,40);
  // Empty categories consume only navigation; back reaches the native menu.
  lists.itemList.entries=[]; lists.itemList.selectedIndex=-1;
  key('down'); assert.equal(lists.itemList.skycrpgCategoryFocus,true);
  assert.equal(key('tab'),false);
  if (platform===0) {
    key(undefined,'keyDown',273); assert.ok(!lists._skycrpgSearchOpen);
    lists.searchWidget.startInput=()=>lists.onSearchInputStart({});
    key(undefined,'keyDown',57);
    assert.equal(lists.itemList.disableInput,true);
    assert.equal(lists._skycrpgFocus._visible,false);
    // Widget inputEnd is an external native callback, modeled here only.
    lists.onSearchInputEnd({data:''});
    assert.equal(lists.itemList.disableInput,false);
    assert.equal(lists._skycrpgFocus._visible,true);
    continue;
  }
  key(undefined,'keyDown',273);
  assert.equal(lists._skycrpgSearchOpen,true);
  assert.equal(lists.itemList.disableInput,true); assert.equal(lists.categoryList.disableSelection,true);
  assert.equal(lists._skycrpgFocus._visible,false);
  assert.equal(lists._skycrpgSearchPad.key0.text,'> A');
  const before=nativeCalls;
  for (const code of [272,274,275,57]) key(undefined,'keyDown',code);
  assert.equal(nativeCalls,before);
  key('enter'); key('enter','keyHold'); key('enter','keyUp');
  assert.equal(lists._skycrpgSearchText,'A');
  key('right'); key('enter'); assert.equal(lists._skycrpgSearchText,'AB');
  assert.equal(lists._skycrpgSearchPad.key1.text,'> B');
  // Every pad cell, including Space/Erase/Clear/Done, is reachable by nav.
  for (let i=0;i<38;i++) key('right');
  assert.equal(lists._skycrpgSearchIndex,39); key('enter');
  assert.equal(lists._nameFilter.filterText,'AB');
  assert.equal(lists._skycrpgSearchPad._visible,false);
  assert.equal(lists.itemList.disableInput,false);
  assert.equal(lists.itemList.selectedIndex,-1);
  for (const cancel of [event('tab'),event('shiftTab'),event(undefined,'keyDown',1)]) {
    key(undefined,'keyDown',273); key('enter');
    lists.handleInput(cancel,path());
    assert.equal(lists._nameFilter.filterText,'AB');
    assert.equal(lists._skycrpgSearchOpen,false);
    assert.equal(lists.itemList.disableInput,false);
  }
  // Space/Erase and the text bound execute real pad operations.
  key(undefined,'keyDown',273);
  for(let i=0;i<36;i++) key('right'); key('enter');
  assert.equal(lists._skycrpgSearchText,'AB ');
  key('right'); key('enter'); assert.equal(lists._skycrpgSearchText,'AB');
  key('tab');
  key(undefined,'keyDown',273);
  for(let i=0;i<40;i++) key('enter');
  assert.equal(lists._skycrpgSearchText.length,32);
  key('tab');
  // Explicit Clear + Done removes even an empty-result filter.
  key(undefined,'keyDown',273);
  for(let i=0;i<38;i++) key('right'); key('enter'); key('right'); key('enter');
  assert.equal(lists._nameFilter.filterText,'');
}
// Native quantity/confirmation cancellation handlers, using their actual bodies.
const cardSource=fs.readFileSync(provider+'/source/actionscript/ItemMenus/ItemCard.as','utf8');
for (const name of ['HandleQuantityMenuInput','HandleConfirmMessageInput']) {
  const events=[];
  const card={HideQuantityMenu(canceled){events.push(['quantity',canceled]);},HideConfirmMessage(){events.push(['confirm']);},
    dispatchEvent(e){events.push(e.type);}};
  method(cardSource,name).call(card,event('tab'));
  assert.ok(events.length>0,name);
  assert.ok(!events.includes('quantitySelect')&&!events.includes('messageConfirm'));
}
// InventoryMenu delegates to native modal before its close path; back from an
// empty category falls through to one native close, for keyboard and controller.
for (const device of ['keyboard','controller']) {
  let closes=0, fades=0;
  global.gfx.io={GameDelegate:{call(){closes++;}}};
  const menu={bFadedIn:true,skycrpgInventoryInput(){},startMenuFade(){fades++;},inventoryLists:{itemList:{disableInput:false}}};
  method(menuSource,'handleInput').call(menu,event('tab'),[{handleInput(){return true;}}]);
  assert.equal(closes,0,device+' modal owns back');
  method(menuSource,'handleInput').call(menu,event('tab'),[{handleInput(){return false;}}]);
  assert.equal(closes,1); assert.equal(fades,1);
}
// Real panel fitting at both displays, scaled HUD separately below, ultrawide
// and nonzero visibleRect origins. Native modal art is stubbed and must be the
// sole visible content layer; rendered fonts/art still require owner captures.
for (const rect of [{x:0,y:0,width:1280,height:720},{x:0,y:0,width:1920,height:1080},
  {x:-320,y:0,width:2560,height:1080},{x:50,y:30,width:1024,height:600}]) {
  global.Stage={visibleRect:rect};
  const lists=listsFixture(), holder=new Clip(), menu=new Clip();
  Object.assign(menu,{inventoryLists:lists,itemCard:{_parent:holder},mouseRotationRect:{},skycrpgDrawEquipment(){}});
  for(const name of ['skycrpgLayout','skycrpgPositionCard']) menu[name]=method(menuSource,name);
  menu.skycrpgLayout();
  const scale=menu._skycrpgScale;
  for(const [clip,w,h] of [[menu._skycrpgPanel,264,550],[lists,542,594],[menu._skycrpgDetailPanel,408,52]]) {
    assert.ok(clip._x>=rect.x+16 && clip._y>=rect.y+16);
    assert.ok(clip._x+w*scale<=rect.x+rect.width-16);
    assert.ok(clip._y+h*scale<=rect.y+rect.height-16);
  }
  const pad=lists._skycrpgSearchPad;
  assert.equal(pad,undefined);
  menu._skycrpgModal=true;menu.skycrpgPositionCard();
  for(const clip of [lists,menu._skycrpgPanel,menu._skycrpgDetailPanel]) assert.equal(clip._visible,false);
  assert.ok(holder.layer>menu._skycrpgModalShade.layer);
  assert.ok(holder._x-15*holder._xscale/100>=rect.x+16);
  assert.ok(holder._x+485*holder._xscale/100<=rect.x+rect.width-16);
  assert.ok(holder._y+50*holder._yscale/100>=rect.y+16);
  assert.ok(holder._y+650*holder._yscale/100<=rect.y+rect.height-16);
  menu._skycrpgModal=false;menu.skycrpgPositionCard();assert.equal(lists._visible,true);
}
// Reuse the HUD contract's transform-aware movie fixtures, injecting the test
// visibleRect into Stage. Initialization/panel functions remain production code.
let hudFixture=fs.readFileSync(root+'/tests/hud_contract.js','utf8').split('const transforms = [')[0];
hudFixture=hudFixture.replace('Stage: {width, height}', 'Stage: {width, height, visibleRect: transform.rect}');
const hudContext={require,process,console};
vm.runInNewContext(hudFixture+'\nthis.fixture={makeMenu,globalBounds,settings};',hudContext);
const {makeMenu,globalBounds,settings}=hudContext.fixture;
for(const rect of [{x:0,y:0,width:1280,height:720},{x:0,y:0,width:1920,height:1080},{x:-320,y:20,width:2560,height:1080}])
  for(const scale of [85,100,115]) for(const offsetX of [-120,0,120]) for(const offsetY of [-60,0,60]) {
    const menu=makeMenu(rect.width,rect.height,settings(scale,offsetX,offsetY),
      {rect,menu:{_x:616.65,_y:475.45,_xscale:80,_yscale:125},ancestor:{_x:20,_y:-30,_xscale:150,_yscale:75}});
    const bounds=[globalBounds(menu.SkyCRPGPartyArea,194,49),globalBounds(menu.SkyCRPGStatus,420,52),globalBounds(menu.SkyCRPGHotbar,536,77)];
    for(const box of bounds) {
      assert.ok(box.x>=rect.x+16-0.001 && box.y>=rect.y+16-0.001);
      assert.ok(box.x+box.width<=rect.x+rect.width-16+0.001);
      assert.ok(box.y+box.height<=rect.y+rect.height-12+0.001);
    }
    assert.ok(bounds[2].y+bounds[2].height+11*scale/100<=bounds[1].y);
    assert.ok(bounds[0].y+bounds[0].height<bounds[2].y);
  }
console.log('PASS: keyboard/controller focus, character pad ownership/cancel, native back, modal separation; viewport margins at 720p/1080p/115%/ultrawide with ancestor transforms');

const longBridge=settings(115,0,0);
const longSlots=Array.from({length:8},()=>({name:'W'.repeat(4000),kind:'WEAPON',equipped:true,count:1}));
longSlots.voiceName='W'.repeat(4000);longSlots.voiceRecovery=4;
longBridge.getHotbar=()=>longSlots;
longBridge.getFollowerStatus=()=>({state:'live',name:'W'.repeat(4000),health:0.5});
const longMenu=makeMenu(1280,720,longBridge);
longMenu.SkyCRPGHotbar.onEnterFrame();longMenu.SkyCRPGPartyArea.onEnterFrame();
assert.equal(longMenu.SkyCRPGHotbar.slot0.name.text,'WWWWWWWWW...');
assert.equal(longMenu.SkyCRPGHotbar.slot0.kind.text,'E WEAPON');
assert.ok(longMenu.SkyCRPGHotbar.voice.text.length<80);
assert.equal(longMenu.SkyCRPGPartyArea.heading.text,'W'.repeat(19)+'...');
assert.equal(longMenu.SkyCRPGPartyArea.availability.text,'HEALTH 50%');
