      // InventoryLists normally offers LEFT/RIGHT to CategoryList first.
      // Route grid arrows before that consumer; never synthesize activation.
      var skycrpgList = this.itemList;
      if(skycrpgList.skycrpgGrid && !skycrpgList.disableInput && !skycrpgList.disableSelection &&
         Shared.GlobalFunc.IsKeyPressed(details))
      {
         var skycrpgNav = details.navEquivalent;
         if(skycrpgList.skycrpgCategoryFocus)
         {
            if(skycrpgNav == gfx.ui.NavigationCode.DOWN || skycrpgNav == gfx.ui.NavigationCode.ENTER)
            {
               skycrpgList.skycrpgCategoryFocus = false;
               skycrpgList.selectDefaultIndex(true);
               return true;
            }
            if(skycrpgNav == gfx.ui.NavigationCode.UP) return true;
            // LEFT/RIGHT fall through to the existing category consumer.
         }
         else
         {
            if(skycrpgNav == gfx.ui.NavigationCode.UP &&
               skycrpgList.getSelectedListEnumIndex() < 3 && skycrpgList.selectedIndex != -1)
            {
               skycrpgList.selectedIndex = -1;
               skycrpgList.skycrpgCategoryFocus = true;
               return true;
            }
            if(skycrpgList.skycrpgMove(details)) return true;
         }
      }
