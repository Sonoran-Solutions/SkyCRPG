   // Only InventoryMenu receives these methods. Native dispatch stays in its
   // existing methods; other item menus and both list definitions stay native.
   var _skycrpgInventoryToken;
   var _skycrpgInventoryIndex;
   var _skycrpgInventoryQuantity = false;
   var _skycrpgInventoryHeld;
   function skycrpgInventoryBridge()
   {
      if(skse == undefined || skse.plugins == undefined) return undefined;
      var bridge = skse.plugins.SkyCRPGUI;
      if(bridge == undefined || typeof bridge.validateItemAction != "function" ||
         typeof bridge.getInventorySnapshot != "function" ||
         typeof bridge.inventoryInputEdge != "function" ||
         typeof bridge.cancelInventoryAction != "function" ||
         typeof bridge.inventoryViewChanged != "function") return undefined;
      return bridge;
   }
   function skycrpgInventoryCapture()
   {
      if(this._skycrpgInventoryQuantity) return;
      var entry = this.inventoryLists.itemList.selectedEntry;
      if(entry == undefined || entry.itemIndex == undefined) return;
      // A repeated highlight or fresh key edge on the same native entry must
      // not mint another token for a refused/consumed selection.
      if(this._skycrpgInventoryToken != undefined && this._skycrpgInventoryIndex == entry.itemIndex) return;
      var bridge = this.skycrpgInventoryBridge();
      if(bridge == undefined) return;
      var snapshot = bridge.getInventorySnapshot();
      this._skycrpgInventoryToken = snapshot == undefined ? undefined : snapshot.token;
      this._skycrpgInventoryIndex = snapshot == undefined ? undefined : snapshot.selectedIndex;
   }
   function skycrpgInventoryInput(details)
   {
      var bridge = this.skycrpgInventoryBridge();
      if(bridge == undefined) return;
      if(this._skycrpgInventoryHeld == undefined) this._skycrpgInventoryHeld = {};
      var key = details.skseKeycode;
      if(key == undefined) return;
      if(details.value == "keyDown")
      {
         if(this._skycrpgInventoryHeld[key] != true)
         {
            this._skycrpgInventoryHeld[key] = true;
            bridge.inventoryInputEdge(key,true);
            this.skycrpgInventoryCapture();
         }
      }
      else if(details.value == "keyUp")
      {
         this._skycrpgInventoryHeld[key] = false;
         bridge.inventoryInputEdge(key,false);
      }
   }
   function skycrpgInventoryAllow(action, amount, phase)
   {
      var bridge = this.skycrpgInventoryBridge();
      if(bridge == undefined) return true;
      // Native callbacks can precede handleInput or highlight notification.
      // Renew only for a different native entry, never the same spent token.
      var entry = this.inventoryLists.itemList.selectedEntry;
      if(this._skycrpgInventoryToken == undefined ||
         (entry != undefined && entry.itemIndex != this._skycrpgInventoryIndex)) this.skycrpgInventoryCapture();
      var allowed = bridge.validateItemAction(this._skycrpgInventoryToken,action,amount,phase) == true;
      if(allowed && phase == "quantity") this._skycrpgInventoryQuantity = true;
      if(phase == "confirm") this._skycrpgInventoryQuantity = false;
      if(!allowed)
      {
         this._skycrpgInventoryQuantity = false;
         this.inventoryLists.itemList.selectedIndex = -1;
         this.inventoryLists.InvalidateListData();
      }
      return allowed;
   }
   function skycrpgInventoryCancel()
   {
      var bridge = this.skycrpgInventoryBridge();
      if(bridge != undefined) bridge.cancelInventoryAction();
      this._skycrpgInventoryQuantity = false;
      this._skycrpgInventoryToken = undefined;
   }
   function skycrpgInventoryViewChanged()
   {
      var bridge = this.skycrpgInventoryBridge();
      if(bridge != undefined) bridge.inventoryViewChanged();
      this._skycrpgInventoryToken = undefined;
      this._skycrpgInventoryQuantity = false;
      // A list refresh is not a physical release of the current gesture.
   }
   function onMouseDown()
   {
      this.skycrpgInventoryInput({skseKeycode:65535,value:"keyDown"});
   }
   function onMouseUp()
   {
      this.skycrpgInventoryInput({skseKeycode:65535,value:"keyUp"});
   }
