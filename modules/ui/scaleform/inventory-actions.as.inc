   // Only InventoryMenu receives these methods. Native dispatch stays in its
   // existing methods; other item menus and both list definitions stay native.
   var _skycrpgInventoryToken;
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
      var bridge = this.skycrpgInventoryBridge();
      if(bridge == undefined) return;
      var snapshot = bridge.getInventorySnapshot();
      this._skycrpgInventoryToken = snapshot == undefined ? undefined : snapshot.token;
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
      // Native mouse callbacks can precede handleInput. First acquisition is
      // allowed; a refused/consumed token is never automatically renewed here.
      if(this._skycrpgInventoryToken == undefined) this.skycrpgInventoryCapture();
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
      this._skycrpgInventoryHeld = {};
   }
   function onMouseDown()
   {
      this.skycrpgInventoryInput({skseKeycode:65535,value:"keyDown"});
   }
   function onMouseUp()
   {
      this.skycrpgInventoryInput({skseKeycode:65535,value:"keyUp"});
   }
