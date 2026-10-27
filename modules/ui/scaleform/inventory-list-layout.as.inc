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
   }
