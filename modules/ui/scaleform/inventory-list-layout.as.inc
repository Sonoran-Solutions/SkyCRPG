   function skycrpgLayout()
   {
      if(!this.itemList.skycrpgGrid) return;
      // Reuse all native controls, processors and filter/sort listeners.
      var panel = this.panelContainer;
      panel.ListBackground._width = 542; panel.ListBackground._height = 570;
      this.categoryList._x = 0; this.categoryList._y = 12;
      // CategoryList derives its selector-rule extent from background width.
      var categoryBackground = this.categoryList.background;
      if(categoryBackground != undefined)
      {
         var categoryRule = categoryBackground.getBounds(panel);
         categoryBackground._width = Math.max(0, categoryBackground._width + 542 - categoryRule.xMax);
      }
      var list = this.itemList;
      list._x = 0; list._y = 126;
      list.listHeight = 444;
      list.header._y = -30;
      list.background._width = 516;
      // Selected category caption occupies the left of the controls row.
      this.categoryLabel._x = 8; this.categoryLabel._y = 67;
      var categories = this.categoryList.getBounds(panel);
      var caption = this.categoryLabel.getBounds(panel);
      if(caption.yMin < categories.yMax + 4)
         this.categoryLabel._y += categories.yMax + 4 - caption.yMin;
      caption = this.categoryLabel.getBounds(panel);
      var background = list.background.getBounds(panel);
      var gear = this.columnSelectButton.getBounds(panel);
      this.columnSelectButton._x += background.xMax - gear.xMax;
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
      var header = list.header.getBounds(panel);
      var rowBottom = Math.max(caption.yMax, search.yMax, gear.yMax);
      if(header.yMin < rowBottom + 4) list.header._y += rowBottom + 4 - header.yMin;
      this._columnSelectDialogX = gear.xMax + 4;
      list.scrollbar._x = 526;
      list.scrollbar.height = 444;
      list.skycrpgFocusOwner = this;
      if(typeof this.skycrpgDrawFocus == "function") this.skycrpgDrawFocus();
      if(this._platform != 0 && typeof this.skycrpgSearchLabel == "function") this.skycrpgSearchLabel(this._nameFilter.filterText);
   }
   function skycrpgDrawFocus()
   {
      if(!this.itemList.skycrpgGrid) return;
      if(this._skycrpgFocus == undefined) this.panelContainer.createEmptyMovieClip("_skycrpgFocus",this.panelContainer.getNextHighestDepth());
      this._skycrpgFocus = this.panelContainer._skycrpgFocus;
      var focus = this._skycrpgFocus;
      focus.clear();
      focus._visible = !this.itemList.disableInput && !this.itemList.disableSelection;
      var panelBounds = this.panelContainer.ListBackground.getBounds(this.panelContainer);
      var listBounds = this.itemList.background.getBounds(this.panelContainer);
      var headerBounds = this.itemList.header.getBounds(this.panelContainer);
      var first = this.itemList.getClipByIndex(0);
      var firstBounds = first != undefined && first._visible ? first.getBounds(this.panelContainer) : listBounds;
      var top = this.itemList.skycrpgCategoryFocus ? 0 : Math.max(panelBounds.yMin, headerBounds.yMax, firstBounds.yMin);
      var bottom = this.itemList.skycrpgCategoryFocus ? 64 : Math.min(panelBounds.yMax, listBounds.yMax);
      top = Math.min(top, bottom);
      var xMin = this.itemList.skycrpgCategoryFocus ? 0 : panelBounds.xMin;
      var xMax = this.itemList.skycrpgCategoryFocus ? 528 : panelBounds.xMax;
      focus.lineStyle(2,0xF0E8D8,100);
      focus.moveTo(xMin,top + 14); focus.lineTo(xMin,top); focus.lineTo(xMin + 14,top);
      focus.moveTo(xMax,bottom - 14); focus.lineTo(xMax,bottom); focus.lineTo(xMax - 14,bottom);
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
      this.itemList.skycrpgCellText(this.panelContainer,"_skycrpgSearchLabel",19000,8,574,510,20,
         text == "" ? hint : "Search: " + text + "  (" + hint + ")",12,0xF0E8D8,false);
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
      pad.clear(); pad.lineStyle(2,0xF0E8D8,100); pad.beginFill(0x16191D,100);
      pad.moveTo(0,0); pad.lineTo(542,0); pad.lineTo(542,570); pad.lineTo(0,570); pad.lineTo(0,0); pad.endFill();
      var list = this.itemList;
      list.skycrpgCellText(pad,"title",900,12,12,518,32,"SEARCH: " + this._skycrpgSearchText,16,0xF0E8D8,false);
      list.skycrpgCellText(pad,"help",901,12,48,518,26,"D-pad: move   A: choose   B: cancel",13,0xF0E8D8,false);
      for(var i = 0; i < 40; i++)
      {
         var label = i < 36 ? "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789".charAt(i) : ["Space","Erase","Clear","Done"][i - 36];
         var selected = i == this._skycrpgSearchIndex;
         var x = 12 + (i % 6) * 86;
         var y = 92 + Math.floor(i / 6) * 62;
         pad.lineStyle(selected ? 3 : 1,0xF0E8D8,100);
         pad.moveTo(x,y); pad.lineTo(x + 80,y); pad.lineTo(x + 80,y + 52); pad.lineTo(x,y + 52); pad.lineTo(x,y);
         list.skycrpgCellText(pad,"key" + i,1000 + i,x + 4,y + 12,72,28,
            (selected ? "> " : "") + label,15,0xF0E8D8,false);
      }
   }
