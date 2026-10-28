   // Player-menu presentation only. No mutation or synthetic activation.
   var _skycrpgEquipmentTimer;
   var _skycrpgRepaintTimer;
   var _skycrpgPanel;
   var _skycrpgModalShade;
   var _skycrpgDetailPanel;
   var _skycrpgModal = false;
   var _skycrpgScale = 1;
   function skycrpgInventoryPresent(snapshot)
   {
      var list = this.inventoryLists.itemList;
      list.skycrpgObservation = snapshot == undefined ? undefined : snapshot.entries;
      if(this._bMenuClosing || this._skycrpgRepaintTimer != undefined) return;
      this._skycrpgRepaintTimer = setInterval(this,"skycrpgRepaintInventory",1);
   }
   function skycrpgRepaintInventory()
   {
      clearInterval(this._skycrpgRepaintTimer);
      delete this._skycrpgRepaintTimer;
      if(this._bMenuClosing) return;
      var list = this.inventoryLists.itemList;
      // A pending native commit includes this repaint; never request its timer.
      if(list._updateRequestID != undefined || list._invalidateRequestID != undefined) return;
      if(typeof list.UpdateList == "function") list.UpdateList();
   }
   function skycrpgLayout()
   {
      var rect = Stage.visibleRect;
      var scale = Math.min(1, (rect.width - 32) / 1280, (rect.height - 32) / 720);
      this._skycrpgScale = scale;
      var left = rect.x + (rect.width - 1280 * scale) / 2;
      var top = rect.y + (rect.height - 720 * scale) / 2;
      var lists = this.inventoryLists;
      lists._xscale = lists._yscale = 100 * scale;
      lists._x = left + 300 * scale; lists._y = top + 30 * scale;
      lists.skycrpgLayout();
      if(this._skycrpgPanel == undefined)
      {
         this.createEmptyMovieClip("_skycrpgPanel", this.getNextHighestDepth());
         this.skycrpgDrawEquipment(undefined);
      }
      this._skycrpgPanel._x = left + 18 * scale; this._skycrpgPanel._y = top + 40 * scale;
      this._skycrpgPanel._xscale = this._skycrpgPanel._yscale = 100 * scale;
      if(this._skycrpgDetailPanel == undefined) this.createEmptyMovieClip("_skycrpgDetailPanel",this.getNextHighestDepth());
      this._skycrpgDetailPanel._x = left + 852 * scale; this._skycrpgDetailPanel._y = top + 40 * scale;
      this._skycrpgDetailPanel._xscale = this._skycrpgDetailPanel._yscale = 100 * scale;
      this.skycrpgPositionCard();
   }
   function UpdateItemCardInfo(info)
   {
      super.UpdateItemCardInfo(info);
      var list = this.inventoryLists.itemList;
      if(this._skycrpgDetailPanel != undefined)
      {
         var entry = list.selectedEntry;
         list.skycrpgCellText(this._skycrpgDetailPanel,"status",900,0,0,408,52,
            entry == undefined ? "Select an item to inspect" : list.skycrpgStatus(entry,true),13,0xF0E8D8,false);
         this._skycrpgDetailPanel.status.multiline = this._skycrpgDetailPanel.status.wordWrap = true;
      }
      this.skycrpgPositionCard();
   }
   function positionFloatingElements()
   {
      super.positionFloatingElements();
      this.skycrpgLayout();
   }
   function skycrpgPositionCard()
   {
      var holder = this.itemCard._parent;
      var rect = Stage.visibleRect;
      var scale = this._skycrpgScale;
      var left = rect.x + (rect.width - 1280 * scale) / 2;
      var top = rect.y + (rect.height - 720 * scale) / 2;
      // Measure local art bounds rather than assuming a particular card frame.
      holder._xscale = holder._yscale = 100;
      var bounds = holder.getBounds(holder);
      var width = Math.max(1, bounds.xMax - bounds.xMin);
      var height = Math.max(1, bounds.yMax - bounds.yMin);
      var fit = this._skycrpgModal ? Math.min(1, 700 / width, 540 / height) : Math.min(1, 408 / width, 510 / height);
      holder._xscale = holder._yscale = fit * scale * 100;
      holder._x = left + ((this._skycrpgModal ? (1280 - width * fit) / 2 : 852) - bounds.xMin * fit) * scale;
      holder._y = top + ((this._skycrpgModal ? (620 - height * fit) / 2 + 30 : 96) - bounds.yMin * fit) * scale;
      // The old preview's mouse rectangle must not sit over the grid.
      this.mouseRotationRect._x = left + 852 * scale;
      this.mouseRotationRect._width = 408 * scale;
      if(this._skycrpgModalShade == undefined)
      {
         this.createEmptyMovieClip("_skycrpgModalShade", this.getNextHighestDepth());
         holder.swapDepths(this.getNextHighestDepth());
      }
      // Native quantity/confirmation owns the only visible interactive layer.
      this.inventoryLists._visible = !this._skycrpgModal;
      if(this._skycrpgPanel != undefined) this._skycrpgPanel._visible = !this._skycrpgModal;
      if(this._skycrpgDetailPanel != undefined) this._skycrpgDetailPanel._visible = !this._skycrpgModal;
      var shade = this._skycrpgModalShade;
      shade.clear(); shade.beginFill(0x080A0D, 96);
      shade.moveTo(rect.x,rect.y); shade.lineTo(rect.x + rect.width,rect.y);
      shade.lineTo(rect.x + rect.width,rect.y + rect.height); shade.lineTo(rect.x,rect.y + rect.height);
      shade.lineTo(rect.x,rect.y); shade.endFill(); shade._visible = this._skycrpgModal;
      if(this._skycrpgModal)
      {
         shade.swapDepths(this.getNextHighestDepth());
         holder.swapDepths(this.getNextHighestDepth());
      }
   }
   function skycrpgRequestEquipment()
   {
      if(this._bMenuClosing) return;
      // Clear stale values at the invalidation boundary; coalesce callback bursts.
      if(this._skycrpgEquipmentTimer == undefined)
      {
         if(this._skycrpgPanel != undefined) this.skycrpgDrawEquipment(undefined);
         this._skycrpgEquipmentTimer = setInterval(this,"skycrpgRefreshEquipment",100);
      }
   }
   function skycrpgRefreshEquipment()
   {
      clearInterval(this._skycrpgEquipmentTimer);
      delete this._skycrpgEquipmentTimer;
      if(this._bMenuClosing || this._skycrpgPanel == undefined) return;
      var bridge = skse == undefined || skse.plugins == undefined ? undefined : skse.plugins.SkyCRPGUI;
      var slots = bridge == undefined || typeof bridge.getInventoryEquipment != "function" ? undefined : bridge.getInventoryEquipment();
      this.skycrpgDrawEquipment(slots);
   }
   function skycrpgDrawEquipment(slots)
   {
      var panel = this._skycrpgPanel;
      if(panel == undefined) return;
      panel.clear(); panel.lineStyle(1,0x82704C,100); panel.beginFill(0x16191D,98);
      panel.moveTo(0,0); panel.lineTo(264,0); panel.lineTo(264,550);
      panel.lineTo(0,550); panel.lineTo(0,0); panel.endFill();
      var list = this.inventoryLists.itemList;
      list.skycrpgCellText(panel,"heading",900,12,8,240,26,"EQUIPMENT",18,0xDFC174,false);
      // Original static silhouette, drawn here; no textures or preview engine.
      panel.lineStyle(3,0x8C8476,100); panel.beginFill(0x34383B,100);
      panel.moveTo(119,48); panel.curveTo(132,34,145,48); panel.curveTo(156,68,132,78);
      panel.curveTo(108,68,119,48); panel.endFill();
      panel.beginFill(0x34383B,100); panel.moveTo(111,85); panel.lineTo(153,85);
      panel.lineTo(175,145); panel.lineTo(160,150); panel.lineTo(149,116);
      panel.lineTo(150,164); panel.lineTo(148,223); panel.lineTo(133,223);
      panel.lineTo(132,174); panel.lineTo(130,223); panel.lineTo(115,223);
      panel.lineTo(114,164); panel.lineTo(115,116); panel.lineTo(104,150);
      panel.lineTo(89,145); panel.lineTo(111,85); panel.endFill();
      var labels = ["HEAD","BODY","HANDS","FEET","LEFT / SHIELD","RIGHT HAND","AMULET","RING"];
      for(var i = 0; i < 8; i++)
      {
         var key = "slot" + i;
         if(panel[key] == undefined) panel.createEmptyMovieClip(key,100 + i);
         var cell = panel[key]; cell._x = 8 + (i % 2) * 126; cell._y = 240 + Math.floor(i / 2) * 62;
         cell.clear(); cell.lineStyle(1,0x696354,100); cell.beginFill(0x202428,100);
         cell.moveTo(0,0); cell.lineTo(122,0); cell.lineTo(122,58); cell.lineTo(0,58); cell.lineTo(0,0); cell.endFill();
         var slot = slots == undefined ? undefined : slots[i];
         var known = slot != undefined && slot.known == true;
         var name = !known ? "Unknown" : (slot.empty == true ? "Empty" : (slot.name == undefined || slot.name == "" ? "Unnamed item" : slot.name));
         list.skycrpgCellText(cell,"label",900,4,2,114,18,labels[i],11,0xDFC174,false);
         list.skycrpgCellText(cell,"name",901,32,20,86,34,name,12,0xF0E8D8,true);
         if(cell.icon == undefined) cell.createEmptyMovieClip("icon",899);
         list.skycrpgTypeIcon(cell.icon,known && slot.empty != true ? slot.formType : undefined);
         cell.icon._x = 3; cell.icon._y = 23; cell.icon._xscale = cell.icon._yscale = 80;
         cell.icon._visible = known && slot.empty != true;
      }
      list.skycrpgCellText(panel,"legend",901,10,496,244,45,"E equipped   F favorite\nS stolen   Q quest   ? unknown",12,0xF0E8D8,true);
      // Legend is explanatory text, not an item name; retain both full lines.
      panel.legend.text = "E equipped   F favorite\nS stolen   Q quest   ? unknown";
   }
   function UpdatePlayerInfo(info)
   {
      super.UpdatePlayerInfo(info);
      this.skycrpgRequestEquipment();
   }
   function skycrpgPresentationClose()
   {
      if(this._skycrpgRepaintTimer != undefined) clearInterval(this._skycrpgRepaintTimer);
      delete this._skycrpgRepaintTimer;
      if(this._skycrpgEquipmentTimer != undefined) clearInterval(this._skycrpgEquipmentTimer);
      delete this._skycrpgEquipmentTimer;
      this.inventoryLists.itemList.skycrpgObservation = undefined;
      if(this._skycrpgPanel != undefined) this._skycrpgPanel._visible = false;
      if(this._skycrpgDetailPanel != undefined) this._skycrpgDetailPanel._visible = false;
      if(this._skycrpgModalShade != undefined) this._skycrpgModalShade._visible = false;
   }
   function onUnload()
   {
      this.skycrpgPresentationClose();
   }
