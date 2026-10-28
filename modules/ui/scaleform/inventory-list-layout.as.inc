   function skycrpgLayout()
   {
      if(!this.itemList.skycrpgGrid) return;
      // Reuse all native controls, processors and filter/sort listeners.
      var panel = this.panelContainer;
      panel.ListBackground._width = 542; panel.ListBackground._height = 570;
      this.categoryList._x = 0; this.categoryList._y = 12;
      this.categoryLabel._x = 8; this.categoryLabel._y = 67;
      this.searchWidget._width = 108;
      this.searchWidget._x = 405; this.searchWidget._y = 70;
      this.columnSelectButton._width = 18;
      this.columnSelectButton._x = 520; this.columnSelectButton._y = 70;
      this._columnSelectDialogX = 210;
      var list = this.itemList;
      list._x = 0; list._y = 126;
      list.listHeight = 444;
      list.header._y = -30;
      list.background._width = 516;
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
      var top = this.itemList.skycrpgCategoryFocus ? 0 : 120;
      var bottom = this.itemList.skycrpgCategoryFocus ? 64 : 570;
      focus.lineStyle(2,0xF0E8D8,100);
      focus.moveTo(0,top + 14); focus.lineTo(0,top); focus.lineTo(14,top);
      focus.moveTo(528,bottom - 14); focus.lineTo(528,bottom); focus.lineTo(514,bottom);
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
