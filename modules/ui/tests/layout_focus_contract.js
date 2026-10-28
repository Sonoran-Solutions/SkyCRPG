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
  return eval('(' + source.slice(start, end).replace(/:\s*(Object|Number|String|Boolean|MovieClip|Array)\b(?=\s*[,;)])/g,'')
    .replace(/super\.(\w+)\(/g,'this.native_$1(') + ')');
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
// Transform-aware bounds: a clip's rectangle lives in its parent's space and
// getBounds(space) maps it through every ancestor's _x/_y/_xscale/_yscale.
function toGlobal(clip,x,y) {
  for (let c=clip;c;c=c._parent) { x=(c._x||0)+x*(c._xscale??100)/100; y=(c._y||0)+y*(c._yscale??100)/100; }
  return [x,y];
}
function fromGlobal(space,x,y) {
  const chain=[]; for (let c=space;c;c=c._parent) chain.unshift(c);
  for (const c of chain) { x=(x-(c._x||0))*100/(c._xscale??100); y=(y-(c._y||0))*100/(c._yscale??100); }
  return [x,y];
}
function mapRect(parent,r,space) {
  const [ax,ay]=fromGlobal(space,...toGlobal(parent,r.xMin,r.yMin)), [bx,by]=fromGlobal(space,...toGlobal(parent,r.xMax,r.yMax));
  return {xMin:Math.min(ax,bx),yMin:Math.min(ay,by),xMax:Math.max(ax,bx),yMax:Math.max(ay,by)};
}
function box(x,y,w,h,parent) {
  return {_x:x,_y:y,_width:w,_height:h,_parent:parent,getBounds(space) {
    const r={xMin:this._x,yMin:this._y,xMax:this._x+this._width,yMax:this._y+this._height};
    return space===undefined ? {xMin:(parent?parent._x:0)+r.xMin,yMin:(parent?parent._y:0)+r.yMin,xMax:(parent?parent._x:0)+r.xMax,yMax:(parent?parent._y:0)+r.yMax} : mapRect(parent,r,space);
  }};
}
// Native 9-slice ListBackground (Nexus inventorylists.swf sprite 93): local
// x 0..562.45, y -377.2..1131.6; _width/_height scale about the origin.
function nineSlice(parent) {
  const local={xMin:0,yMin:-377.2,xMax:562.45,yMax:1131.6}, clip={_x:0,_y:0,_parent:parent,sx:1,sy:1};
  Object.defineProperty(clip,'_width',{get(){return (local.xMax-local.xMin)*this.sx;},set(v){this.sx=v/(local.xMax-local.xMin);}});
  Object.defineProperty(clip,'_height',{get(){return (local.yMax-local.yMin)*this.sy;},set(v){this.sy=v/(local.yMax-local.yMin);}});
  clip.getBounds=function(space){return mapRect(parent,{xMin:this._x+local.xMin*this.sx,yMin:this._y+local.yMin*this.sy,
    xMax:this._x+local.xMax*this.sx,yMax:this._y+local.yMax*this.sy},space);};
  return clip;
}
// Grid tiles as ScrollingList places them on 172 x 74 spacing. Their getBounds
// is inflated like the native entry clip's hidden children (an equip icon at
// x -30.5, a 527-unit background); the painted cell is 165x68.
function tiles(list,count) {
  const clips=Array.from({length:18},(_,i)=>{
    const clip=box((i%3)*172,Math.floor(i/3)*74,165,68,list);
    const painted=clip.getBounds;
    clip.getBounds=function(space){const b=painted.call(this,space);return {xMin:b.xMin-30.5,yMin:b.yMin,xMax:b.xMin+527,yMax:b.yMax};};
    return Object.assign(clip,{_visible:i<count});
  });
  list.getClipByIndex=i=>clips[i];
  return clips;
}
// Native header: labels plus a rule (`seperator`) that starts at x -4 and is
// wider than the panel; getBounds is the union of both, like a GFx sprite.
function headerStub(list) {
  const header={_x:0,_y:-30,_parent:list};
  header.labels=box(0,0,516,20,header); header.seperator=box(-4,20,563,2,header);
  header.getBounds=function(space){const a=this.labels.getBounds(space),b=this.seperator.getBounds(space);
    return {xMin:Math.min(a.xMin,b.xMin),yMin:Math.min(a.yMin,b.yMin),xMax:Math.max(a.xMax,b.xMax),yMax:Math.max(a.yMax,b.yMax)};};
  return header;
}
// Inner edges of the native frame lines (3.05..5.1 inside each backdrop edge).
function frameContent(panel) {
  const b=panel.ListBackground.getBounds(panel);
  return {left:b.xMin+5.1+8,right:b.xMax-5.1-8};
}
function inside(bounds,content,label) {
  assert.ok(bounds.xMin>=content.left-1e-6 && bounds.xMax<=content.right+1e-6, label+' inside the frame padding '+JSON.stringify([bounds,content]));
}
function nearPoints(actual,expected) { actual.flat().forEach((v,i)=>assert.ok(Math.abs(v-expected.flat()[i])<1e-6,JSON.stringify([actual,expected]))); }
function intersects(a,b) { return a.xMin < b.xMax && b.xMin < a.xMax && a.yMin < b.yMax && b.yMin < a.yMax; }
function listsFixture() {
  const list = {skycrpgGrid:true,selectedIndex:10,skycrpgCategoryFocus:false,
    entries:[{itemIndex:10},{itemIndex:40},{itemIndex:20},{itemIndex:70}],
    getListEnumSize(){return this.entries.length;},
    getSelectedListEnumIndex(){return this.entries.findIndex(e=>e.itemIndex===this.selectedIndex);},
    getListEnumEntry(i){return this.entries[i];},
    doSetSelectedIndex(i){this.selectedIndex=i;},selectDefaultIndex(){this.selectedIndex=this.entries[0].itemIndex;},
    header:{},background:{},scrollbar:{},getClipByIndex(){return undefined;}};
  for (const name of ['skycrpgMove','skycrpgCellText']) list[name] = method(gridSource,name);
  const lists = {_currentState:1,_platform:1,_sortOrderKey:272,_searchKey:57,_x:0,_y:0,
    itemList:list,panelContainer:new Clip(),_nameFilter:{filterText:''}};
  // Native offsets: panelContainer sits 25 units into InventoryLists, and the
  // scrollbar and header rule reach past the 542-unit backdrop.
  const panel=lists.panelContainer; Object.assign(panel,{_x:25,_y:0,_parent:lists});
  Object.assign(list,{_x:0,_y:0,_parent:panel});
  list.header=headerStub(list); list.background=box(0,0,516,444,list); list.scrollbar=box(534.5,35,14,444,list);
  Object.assign(lists,{categoryList:box(0,12,510,44,panel),categoryLabel:box(8,67,145,22,panel),
    searchWidget:box(0,0,140,20,panel),columnSelectButton:box(0,0,18,20,panel)});
  lists.categoryList.handleInput=()=>false;
  panel.ListBackground = nineSlice(panel);
  lists.categoryList.background = box(0,0,560,2,lists.categoryList);
  for (const name of ['skycrpgLayout','skycrpgDrawFocus','skycrpgPanelExtent','skycrpgGridBounds','skycrpgSearchInput','skycrpgDrawSearch',
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
  assert.ok(lists._skycrpgFocus.points.some(([x,y])=>Math.abs(x-lists._skycrpgContent.left)<1e-9&&y===0));
  key('down'); assert.equal(lists.itemList.selectedIndex,10);
  assert.equal(lists.itemList.skycrpgCategoryFocus,false);
  assert.ok(lists._skycrpgFocus.points.some(([x,y])=>Math.abs(x-lists._skycrpgContent.left)<1e-9&&y===126));
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
// Stub bounds model the round-3 owner captures: panelContainer +25, a scrollbar
// and header rule past 542, a 9-slice backdrop shorter than the six-row grid.
let minTile=Infinity;
for (const rect of [{x:0,y:0,width:1280,height:720},{x:0,y:0,width:1920,height:1080},
  {x:-320,y:0,width:2560,height:1080},{x:50,y:30,width:1024,height:600}]) {
  global.Stage={visibleRect:rect};
  const lists=listsFixture(), holder=new Clip(), menu=new Clip();
  const list=lists.itemList, panel=lists.panelContainer;
  Object.assign(menu,{inventoryLists:lists,itemCard:{_parent:holder},mouseRotationRect:{Lock(){}},bottomBar:{getBounds(){return {yMin:rect.y+rect.height-50};}},skycrpgDrawEquipment(){}});
  lists._parent=menu;
  for(const name of ['skycrpgLayout','skycrpgPositionCard','skycrpgModelColumn','skycrpgSyncStatus','skycrpgRefreshItemPreview','skycrpgPlaceItemPreview','UpdateItemCardInfo']) menu[name]=method(menuSource,name);
  menu.native_UpdateItemCardInfo=()=>{};
  const tileClips=tiles(list,18);
  menu.skycrpgLayout();
  const scale=menu._skycrpgScale;
  minTile=Math.min(minTile,165*scale);
  for(const [clip,w,h] of [[menu._skycrpgPanel,264,550],[lists,542,594]]) {
    assert.ok(clip._x>=rect.x+16 && clip._y>=rect.y+16);
    assert.ok(clip._x+w*scale<=rect.x+rect.width-16);
    assert.ok(clip._y+h*scale<=rect.y+rect.height-16);
  }
  // B: the measured visible panel (frame, scrollbar, header rule) ends at least
  // 16 units left of the engine model column; the column is the plugin's target.
  const real=lists.skycrpgPanelExtent(menu), model=menu.skycrpgModelColumn();
  assert.ok(real.xMax>lists._x+(542+20)*scale, 'stub extras are wider than 542');
  assert.ok(real.xMax<=model.left-16+0.001, 'panel real right edge clear of the model column');
  assert.ok(Math.abs(model.left-(rect.x+0.66*rect.width))<1e-9 && Math.abs(model.right-(rect.x+rect.width-16))<1e-9);
  assert.ok(menu._skycrpgPanel._x+264*scale<=lists._x);
  const columnLeft=real.xMax+16, columnRight=rect.x+rect.width-16;
  assert.ok(Math.abs(menu.mouseRotationRect._x-columnLeft)<1e-9);
  const preview={xMin:columnLeft,xMax:rect.x+rect.width,yMin:rect.y,yMax:rect.y+rect.height*0.55};
  // C: the card lies in the fixed card column, below the band, above the bar.
  const cardBox=()=>({xMin:holder._x-15*holder._xscale/100,xMax:holder._x+485*holder._xscale/100,yMin:holder._y+50*holder._yscale/100,yMax:holder._y+650*holder._yscale/100});
  const card=cardBox();
  assert.ok(card.xMin>=columnLeft-0.001 && card.xMax<=columnRight+0.001, 'card inside its column');
  assert.ok(card.yMin>=preview.yMax && !intersects(card,preview) && !intersects(card,real));
  assert.ok(card.yMax<=rect.y+rect.height-50-8*scale+0.001);
  // A consistent centre whatever the card's own width.
  const centre=(card.xMin+card.xMax)/2;
  assert.ok(Math.abs(centre-(columnLeft+columnRight)/2)<0.001);
  holder.getBounds=()=>({xMin:40,yMin:50,xMax:300,yMax:650});menu.skycrpgPositionCard();
  assert.ok(Math.abs((holder._x+170*holder._xscale/100)-centre)<0.001, 'narrow card keeps the centre');
  delete holder.getBounds;menu.skycrpgPositionCard();
  // D: the status line is above the card, centred with it, inside the column.
  list.selectedIndex=10; list.selectedEntry={itemIndex:10,count:1,isQuestItem:true,equipState:0,favorite:false,isStolen:false};
  list.skycrpgStatus=method(gridSource,'skycrpgStatus');
  menu._bItemCardFadedIn=true;
  menu.UpdateItemCardInfo({});
  const detail=menu._skycrpgDetailPanel;
  assert.equal(detail.status.text,'Quest item');
  const status={xMin:detail._x,xMax:detail._x+detail.status._width*scale,yMin:detail._y,yMax:detail._y+28*scale};
  assert.ok(status.xMin>=columnLeft-0.001 && status.xMax<=columnRight+0.001, 'status inside the card column');
  assert.ok(Math.abs((status.xMin+status.xMax)/2-centre)<0.001, 'status centred with the card');
  assert.ok(status.yMin>=preview.yMax && !intersects(status,real) && !intersects(status,cardBox()) && status.yMax<=cardBox().yMin+0.001);
  assert.equal(detail._visible,true);
  // Hidden with the card: faded card, category focus, no selection, empty list, modal.
  menu._bItemCardFadedIn=false;menu.skycrpgSyncStatus();assert.equal(detail._visible,false);
  menu._bItemCardFadedIn=true;menu.skycrpgSyncStatus();assert.equal(detail._visible,true);
  list.skycrpgCategoryFocus=true;lists.skycrpgDrawFocus();assert.equal(detail._visible,false);
  list.skycrpgCategoryFocus=false;lists.skycrpgDrawFocus();assert.equal(detail._visible,true);
  list.selectedIndex=-1;menu.skycrpgSyncStatus();assert.equal(detail._visible,false);
  list.selectedIndex=10;const entries=list.entries;list.entries=[];menu.skycrpgSyncStatus();assert.equal(detail._visible,false);
  list.entries=entries;menu.skycrpgSyncStatus();assert.equal(detail._visible,true);
  // A: the plugin is told to centre the preview over the card (stage units),
  // then SkyUI's positionFloatingElements reloads the model while a card shows.
  const delegated=[], placed=[];
  global.skse={plugins:{SkyCRPGUI:{placeItemPreview(c,h){placed.push([c,h]);return true;}}}};
  menu.positionFloatingElements=()=>delegated.push('positionFloatingElements');
  menu._bItemCardPositioned=true;menu.skycrpgRefreshItemPreview();
  assert.deepEqual(delegated,['positionFloatingElements']);
  assert.equal(placed.length,1);
  assert.ok(Math.abs(placed[0][0]-centre)<1e-9 && Math.abs(placed[0][1]-(columnRight-centre))<1e-9, 'preview centred over the card');
  menu._skycrpgModal=true;menu.skycrpgRefreshItemPreview();menu._skycrpgModal=false;
  list.selectedIndex=-1;menu.skycrpgRefreshItemPreview();
  assert.equal(delegated.length,1);assert.equal(placed.length,2);list.selectedIndex=10;
  delete global.skse.plugins.SkyCRPGUI.placeItemPreview;menu.skycrpgPlaceItemPreview();assert.equal(placed.length,2);
  // E: the backdrop and its frame cover the whole grid; the bracket hugs tiles.
  const backdrop=panel.ListBackground.getBounds(panel);
  const content=frameContent(panel), L=list._x;
  const firstTile={xMin:L,yMin:126,xMax:L+165,yMax:126+68}, lastTile={xMin:L+344,yMin:126+370,xMax:L+509,yMax:126+370+68};
  assert.ok(Math.abs(L-content.left)<1e-9, 'tiles start at the frame padding');
  assert.ok(backdrop.yMax>=list._y+444 && backdrop.yMax>=lastTile.yMax, 'backdrop covers the six-row viewport');
  // B (round 5): every panel child sits inside the frame lines with 8 units' padding.
  for(const tile of tileClips) if(tile._visible) inside({xMin:L+tile._x,xMax:L+tile._x+165},content,'tile');
  for(const [clip,label] of [[lists.categoryList,'category bar'],[lists.categoryList.background,'category rule'],[list.header,'header and rule'],
    [list.scrollbar,'scrollbar'],[lists.searchWidget,'search'],[lists.columnSelectButton,'gear'],[lists.categoryLabel,'category caption']]) inside(clip.getBounds(panel),content,label);
  assert.ok(Math.abs(list.header.getBounds(panel).xMax-content.right)<1e-6, 'header rule reaches the padding');
  assert.ok(Math.abs(lists.categoryList.background.getBounds(panel).xMax-content.right)<1e-6, 'category rule reaches the padding');
  assert.ok(Math.abs(lists.columnSelectButton.getBounds(panel).xMax-content.right)<1e-6, 'gear at the right padding');
  assert.ok(backdrop.yMin<0, 'native backdrop top retained');
  lists.skycrpgDrawFocus();
  let region=lists._skycrpgFocus.points;
  nearPoints([region[1],region[4]],[[firstTile.xMin,firstTile.yMin],[lastTile.xMax,lastTile.yMax]]);
  inside({xMin:region[1][0],xMax:region[4][0]},content,'grid bracket');
  assert.ok(region[1][1]>=list.header.getBounds(panel).yMax);
  // The category bracket uses the same padding.
  list.skycrpgCategoryFocus=true;lists.skycrpgDrawFocus();region=lists._skycrpgFocus.points;
  nearPoints([region[1],region[4]],[[content.left,0],[content.right,64]]);
  list.skycrpgCategoryFocus=false;lists.skycrpgDrawFocus();region=lists._skycrpgFocus.points;
  // Layout runs on every positionFloatingElements; it must be idempotent.
  menu.skycrpgLayout();
  const again=panel.ListBackground.getBounds(panel);
  for(const k of ['xMin','yMin','xMax','yMax']) assert.ok(Math.abs(again[k]-backdrop[k])<1e-9);
  // One favourite: the bracket is that tile; no tile: the six-row viewport.
  tiles(list,1);lists.skycrpgDrawFocus();region=lists._skycrpgFocus.points;
  nearPoints([region[1],region[4]],[[firstTile.xMin,firstTile.yMin],[firstTile.xMax,firstTile.yMax]]);
  tiles(list,0);lists.skycrpgDrawFocus();region=lists._skycrpgFocus.points;
  nearPoints([region[1],region[4]],[[L,126],[L+509,126+370+68]]);
  tiles(list,18);
  assert.ok(lists.categoryList.background.getBounds(panel).xMax<=backdrop.xMax+0.001);
  for(const clip of [lists.categoryList.background,list.header,lists.categoryList,list.scrollbar]) assert.ok(clip.getBounds(menu).xMax<=model.left-16+0.001);
  const search=lists.searchWidget.getBounds(panel), gear=lists.columnSelectButton.getBounds(panel), header=list.header.getBounds(panel), caption=lists.categoryLabel.getBounds(panel);
  for(const [a,b] of [[search,gear],[search,header],[gear,header],[search,caption],[caption,lists.categoryList.getBounds(panel)]]) assert.ok(!intersects(a,b));
  assert.ok(Math.abs(search.yMax-caption.yMax)<0.001 && Math.abs(gear.yMax-caption.yMax)<0.001);
  const pad=lists._skycrpgSearchPad;
  assert.equal(pad,undefined);
  // Modal quantity/confirmation layout is unchanged.
  menu._skycrpgModal=true;menu.skycrpgPositionCard();
  const modalScale=Math.min(1,(rect.width-32)/1280,(rect.height-32)/720), fit=Math.min(1,700/500,540/600);
  assert.ok(Math.abs(holder._xscale/100-fit*modalScale)<1e-9);
  const modalLeft=rect.x+(rect.width-1280*modalScale)/2, modalTop=rect.y+(rect.height-720*modalScale)/2;
  assert.ok(Math.abs(holder._x-(modalLeft+((1280-500*fit)/2+15*fit)*modalScale))<1e-9);
  assert.ok(Math.abs(holder._y-(modalTop+((620-600*fit)/2+30-50*fit)*modalScale))<1e-9);
  for(const clip of [lists,menu._skycrpgPanel,menu._skycrpgDetailPanel]) assert.equal(clip._visible,false);
  assert.ok(holder.layer>menu._skycrpgModalShade.layer);
  assert.ok(holder._x-15*holder._xscale/100>=rect.x+16);
  assert.ok(holder._x+485*holder._xscale/100<=rect.x+rect.width-16);
  assert.ok(holder._y+50*holder._yscale/100>=rect.y+16);
  assert.ok(holder._y+650*holder._yscale/100<=rect.y+rect.height-16);
  menu._skycrpgModal=false;menu.skycrpgPositionCard();
  assert.equal(lists._visible,true);assert.equal(menu._skycrpgDetailPanel._visible,true);
}
// F: the native bottom bar shows R3 (SKSE 273) for search on gamepad in the
// player grid, and keeps the configured keyboard key on PC or in other lists.
for (const [platform,grid,expected] of [[1,true,273],[0,true,57],[1,false,57]]) {
  const buttons=[];
  const bar={_platform:platform,_searchControls:{keyCode:57},_cancelControls:{},_sortColumnControls:[],_sortOrderControls:{},_switchControls:{},
    inventoryLists:{itemList:{skycrpgGrid:grid}},navPanel:{clearButtons(){},addButton(b){buttons.push(b);},updateButtons(){}}};
  method(menuSource,'updateBottomBar').call(bar,false);
  assert.equal(buttons.find(b=>b.text==='$Search').controls.keyCode,expected);
}
// Minimum rendered tile width across the four rectangles (1024x600 is the floor).
assert.ok(minTile>=120, 'minimum rendered tile width '+minTile);
console.log('PASS: measured panel extent vs model column, fixed card/status column, status hidden with card, backdrop covers grid, tile-hugging bracket, R3 search button; minimum tile '+minTile.toFixed(1));
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
