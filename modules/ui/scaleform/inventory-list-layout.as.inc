   // Native panel art (Nexus inventorylists.swf sprite 93 / shape 92): a 9-slice
   // backdrop whose two vertical frame lines sit 3.05..5.1 units inside its left
   // and right edges, in the fixed 18-unit borders. Content keeps 8 more units.
   var _skycrpgContent;
   function skycrpgLayout()
   {
      if(!this.itemList.skycrpgGrid) return;
      // Reuse all native controls, processors and filter/sort listeners.
      var panel = this.panelContainer;
      var backdrop = panel.ListBackground;
      var list = this.itemList;
      var inset = 5.1 + 8;
      backdrop._x = 0; backdrop._width = 542;
      var left = backdrop.getBounds(panel).xMin + inset;
      // Three 165-unit tiles on 172 spacing start inside the left frame padding.
      list._x = left; list._y = 126;
      list.listHeight = 444;
      list.background._width = 509;
      list.scrollbar._x = 509 + 6;
      list.scrollbar.height = 444;
      // Measure the header labels without the native rule, then keep them inside.
      var rule = list.header.seperator;
      if(rule != undefined) rule._width = 1;
      list.header._x = 0; list.header._y = -30;
      var header = list.header.getBounds(panel);
      if(header.xMin < left) list.header._x += left - header.xMin;
      header = list.header.getBounds(panel);
      var scroll = list.scrollbar.getBounds(panel);
      // Widen the backdrop (and so its right frame line) to hold the content.
      var contentRight = Math.max(left + 509, scroll.xMax, header.xMax);
      backdrop._width = Math.max(542, contentRight + inset - backdrop.getBounds(panel).xMin);
      var right = backdrop.getBounds(panel).xMax - inset;
      this._skycrpgContent = {left:left,right:right};
      if(rule != undefined)
      {
         // The rule spans the padded content: from `left` to `right`.
         var ruleBounds = rule.getBounds(panel);
         if(ruleBounds.xMin < left) rule._x += left - ruleBounds.xMin;
         rule._width += right - rule.getBounds(panel).xMax;
      }
      // CategoryList spreads its icons and selector rule over background width.
      this.categoryList._x = left; this.categoryList._y = 12;
      var categoryBackground = this.categoryList.background;
      if(categoryBackground != undefined)
      {
         var categoryRule = categoryBackground.getBounds(panel);
         categoryBackground._width = Math.max(0, categoryBackground._width + right - categoryRule.xMax);
         if(typeof this.categoryList.UpdateList == "function") this.categoryList.UpdateList();
      }
      // Selected category caption occupies the left of the controls row.
      this.categoryLabel._x = left; this.categoryLabel._y = 67;
      var categories = this.categoryList.getBounds(panel);
      var caption = this.categoryLabel.getBounds(panel);
      if(caption.yMin < categories.yMax + 4)
         this.categoryLabel._y += categories.yMax + 4 - caption.yMin;
      caption = this.categoryLabel.getBounds(panel);
      var gear = this.columnSelectButton.getBounds(panel);
      this.columnSelectButton._x += right - gear.xMax;
      gear = this.columnSelectButton.getBounds(panel);
      if(this._skycrpgSearchWidth == undefined) this._skycrpgSearchWidth = this.searchWidget._width;
      var room = Math.max(48, gear.xMin - 8 - caption.xMax - 8);
      this.searchWidget._width = Math.min(this._skycrpgSearchWidth, room);
      var search = this.searchWidget.getBounds(panel);
      this.searchWidget._x += gear.xMin - 8 - search.xMax;
      search = this.searchWidget.getBounds(panel);
      // Align visual bottom edges, then move the native header beneath the row.
      this.searchWidget._y += caption.yMax - search.yMax;
      this.columnSelectButton._y += caption.yMax - gear.yMax;
      search = this.searchWidget.getBounds(panel);
      gear = this.columnSelectButton.getBounds(panel);
      header = list.header.getBounds(panel);
      var rowBottom = Math.max(caption.yMax, search.yMax, gear.yMax);
      if(header.yMin < rowBottom + 4) list.header._y += rowBottom + 4 - header.yMin;
      this._columnSelectDialogX = gear.xMax + 4;
      // The 9-slice backdrop and its frame cover the whole six-row viewport.
      // Reset first so repeated layouts are idempotent; keep the native top.
      backdrop._y = 0; backdrop._height = 570;
      var cover = backdrop.getBounds(panel);
      var gridBottom = list._y + 444 + 8;
      if(cover.yMax < gridBottom)
      {
         backdrop._height += gridBottom - cover.yMax;
         backdrop._y += cover.yMin - backdrop.getBounds(panel).yMin;
      }
      list.skycrpgFocusOwner = this;
      if(typeof this.skycrpgDrawFocus == "function") this.skycrpgDrawFocus();
      if(this._platform != 0 && typeof this.skycrpgSearchLabel == "function") this.skycrpgSearchLabel(this._nameFilter.filterText);
   }
   // The panel's visible extent in `space` coordinates: the backdrop and its
   // frame lines, plus the scrollbar, search and gear. Header, category bar and
   // tiles are laid out inside the frame; category/header clips are excluded
   // because hidden native children can inflate their getBounds.
   function skycrpgPanelExtent(space)
   {
      var list = this.itemList;
      var clips = [this.panelContainer.ListBackground, list.scrollbar, this.searchWidget, this.columnSelectButton];
      var extent;
      for(var i = 0; i < clips.length; i++)
      {
         if(clips[i] == undefined || typeof clips[i].getBounds != "function") continue;
         var b = clips[i].getBounds(space);
         if(extent == undefined) extent = {xMin:b.xMin,yMin:b.yMin,xMax:b.xMax,yMax:b.yMax};
         else
         {
            extent.xMin = Math.min(extent.xMin,b.xMin); extent.yMin = Math.min(extent.yMin,b.yMin);
            extent.xMax = Math.max(extent.xMax,b.xMax); extent.yMax = Math.max(extent.yMax,b.yMax);
         }
      }
      return extent;
   }
   // Visible tiles, or the six-row viewport when no tile is shown.
   function skycrpgGridBounds()
   {
      var list = this.itemList;
      var grid;
      for(var i = 0; i < 18; i++)
      {
         var clip = list.getClipByIndex(i);
         if(clip == undefined || !clip._visible) continue;
         // The painted 165x68 cell, not getBounds: hidden native entry children
         // (a 527-unit background, an equip icon at x -30) inflate clip bounds.
         var b = {xMin:list._x + clip._x,yMin:list._y + clip._y,xMax:list._x + clip._x + 165,yMax:list._y + clip._y + 68};
         if(grid == undefined) grid = {xMin:b.xMin,yMin:b.yMin,xMax:b.xMax,yMax:b.yMax};
         else
         {
            grid.xMin = Math.min(grid.xMin,b.xMin); grid.yMin = Math.min(grid.yMin,b.yMin);
            grid.xMax = Math.max(grid.xMax,b.xMax); grid.yMax = Math.max(grid.yMax,b.yMax);
         }
      }
      if(grid == undefined) grid = {xMin:list._x,yMin:list._y,xMax:list._x + 2 * 172 + 165,yMax:list._y + 5 * 74 + 68};
      return grid;
   }
   function skycrpgDrawFocus()
   {
      if(!this.itemList.skycrpgGrid) return;
      if(this._skycrpgFocus == undefined) this.panelContainer.createEmptyMovieClip("_skycrpgFocus",this.panelContainer.getNextHighestDepth());
      this._skycrpgFocus = this.panelContainer._skycrpgFocus;
      var focus = this._skycrpgFocus;
      focus.clear();
      focus._visible = !this.itemList.disableInput && !this.itemList.disableSelection;
      // The grid region hugs the visible tiles, never the backdrop.
      var grid = this.skycrpgGridBounds();
      var top = this.itemList.skycrpgCategoryFocus ? 0 : grid.yMin;
      var bottom = this.itemList.skycrpgCategoryFocus ? 64 : grid.yMax;
      var content = this._skycrpgContent == undefined ? {left:0,right:528} : this._skycrpgContent;
      var xMin = this.itemList.skycrpgCategoryFocus ? content.left : grid.xMin;
      var xMax = this.itemList.skycrpgCategoryFocus ? content.right : grid.xMax;
      focus.lineStyle(2,0xF0E8D8,100);
      focus.moveTo(xMin,top + 14); focus.lineTo(xMin,top); focus.lineTo(xMin + 14,top);
      focus.moveTo(xMax,bottom - 14); focus.lineTo(xMax,bottom); focus.lineTo(xMax - 14,bottom);
      // Focus changes (category bar, search, empty list) also gate the status line.
      if(this._skycrpgMenu != undefined && typeof this._skycrpgMenu.skycrpgSyncStatus == "function") this._skycrpgMenu.skycrpgSyncStatus();
   }
   // A bounded controller character pad uses the existing native name filter.
   // No pointer emulation, text-entry engine hook or item dispatch is involved.
   var _skycrpgSearchOpen = false;
   var _skycrpgSearchIndex = 0;
   var _skycrpgSearchText = "";
   var _skycrpgSearchPrior = "";
   function skycrpgSearchInput(details)
   {
      var list = this.itemList;
      if(!list.skycrpgGrid) return false;
      var pressed = Shared.GlobalFunc.IsKeyPressed(details);
      if(!this._skycrpgSearchOpen)
      {
         // Right-thumb click is unused by the pinned player InventoryMenu.
         if(this._platform == 0 || details.skseKeycode != 273 || details.value != "keyDown" ||
            list.disableInput || list.disableSelection || this.categoryList.disableInput) return false;
         this._skycrpgSearchPrior = this._nameFilter.filterText == undefined ? "" : this._nameFilter.filterText;
         this._skycrpgSearchText = this._skycrpgSearchPrior;
         this._skycrpgSearchIndex = 0;
         this._skycrpgSearchOpen = true;
         this.onSearchInputStart({});
         this.skycrpgDrawFocus();
         this.skycrpgDrawSearch();
         return true;
      }
      if(!pressed) return true;
      var nav = details.navEquivalent;
      if(nav == gfx.ui.NavigationCode.TAB || nav == gfx.ui.NavigationCode.SHIFT_TAB ||
         details.skseKeycode == 1)
      {
         this.skycrpgEndSearch(false);
         return true;
      }
      var index = this._skycrpgSearchIndex;
      if(nav == gfx.ui.NavigationCode.UP) index -= 6;
      else if(nav == gfx.ui.NavigationCode.DOWN) index += 6;
      else if(nav == gfx.ui.NavigationCode.LEFT) index--;
      else if(nav == gfx.ui.NavigationCode.RIGHT) index++;
      else if(nav == gfx.ui.NavigationCode.ENTER && details.value == "keyDown")
      {
         if(index == 39) { this.skycrpgEndSearch(true); return true; }
         if(index == 38) this._skycrpgSearchText = "";
         else if(index == 37) this._skycrpgSearchText = this._skycrpgSearchText.substr(0,Math.max(0,this._skycrpgSearchText.length - 1));
         else if(this._skycrpgSearchText.length < 32)
            this._skycrpgSearchText += index == 36 ? " " : "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789".charAt(index);
      }
      this._skycrpgSearchIndex = Math.max(0,Math.min(39,index));
      this.skycrpgDrawSearch();
      return true;
   }
   function skycrpgEndSearch(accept)
   {
      var text = accept ? this._skycrpgSearchText : this._skycrpgSearchPrior;
      this._skycrpgSearchOpen = false;
      this._skycrpgSearchPad._visible = false;
      this.onSearchInputEnd({data:text});
      this.itemList.skycrpgCategoryFocus = true;
      this.itemList.selectedIndex = -1;
      this.skycrpgDrawFocus();
      this.skycrpgSearchLabel(text);
   }
   function skycrpgSearchLabel(text)
   {
      if(!this.itemList.skycrpgGrid) return;
      if(text == undefined) text = "";
      var hint = this._platform == 0 ? "Space: search" : "R3: search / clear";
      var content = this._skycrpgContent == undefined ? {left:8,right:518} : this._skycrpgContent;
      this.itemList.skycrpgCellText(this.panelContainer,"_skycrpgSearchLabel",19000,content.left,574,content.right - content.left,20,
         text == "" ? hint : "Search: " + text + "  (" + hint + ")",12,0xF0E8D8,false);
      this.panelContainer._skycrpgSearchLabel._x = content.left;
      this.panelContainer._skycrpgSearchLabel._width = content.right - content.left;
   }
   function skycrpgDrawSearch()
   {
      if(this._skycrpgSearchPad == undefined)
      {
         this._skycrpgSearchPad = this.panelContainer.createEmptyMovieClip("_skycrpgSearchPad",19001);
         this._skycrpgSearchPad.onPress = function() {};
         this._skycrpgSearchPad.useHandCursor = false;
      }
      var pad = this._skycrpgSearchPad;
      pad._visible = true;
      // The pad stays inside the frame padding like every other panel child.
      var content = this._skycrpgContent == undefined ? {left:0,right:542} : this._skycrpgContent;
      var width = content.right - content.left;
      pad._x = content.left;
      pad.clear(); pad.lineStyle(2,0xF0E8D8,100); pad.beginFill(0x16191D,100);
      pad.moveTo(0,0); pad.lineTo(width,0); pad.lineTo(width,570); pad.lineTo(0,570); pad.lineTo(0,0); pad.endFill();
      var list = this.itemList;
      list.skycrpgCellText(pad,"title",900,12,12,width - 24,32,"SEARCH: " + this._skycrpgSearchText,16,0xF0E8D8,false);
      list.skycrpgCellText(pad,"help",901,12,48,width - 24,26,"D-pad: move   A: choose   B: cancel",13,0xF0E8D8,false);
      for(var i = 0; i < 40; i++)
      {
         var label = i < 36 ? "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789".charAt(i) : ["Space","Erase","Clear","Done"][i - 36];
         var selected = i == this._skycrpgSearchIndex;
         var x = 12 + (i % 6) * 84;
         var y = 92 + Math.floor(i / 6) * 62;
         pad.lineStyle(selected ? 3 : 1,0xF0E8D8,100);
         pad.moveTo(x,y); pad.lineTo(x + 78,y); pad.lineTo(x + 78,y + 52); pad.lineTo(x,y + 52); pad.lineTo(x,y);
         list.skycrpgCellText(pad,"key" + i,1000 + i,x + 4,y + 12,70,28,
            (selected ? "> " : "") + label,15,0xF0E8D8,false);
      }
   }
