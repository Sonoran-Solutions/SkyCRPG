      if (this._skycrpgCompassRelease != undefined) this._skycrpgCompassRelease();
      if (this.SkyCRPGCompassWatch != undefined) this.SkyCRPGCompassWatch.removeMovieClip();
      this._skycrpgCompassRelease = function() {
         var compass = this.CompassShoutMeterHolder == undefined ? undefined : this.CompassShoutMeterHolder.Compass;
         if (this._skycrpgCompassOwned == true && compass == this._skycrpgCompassClip && compass._visible == false)
            compass._visible = this._skycrpgCompassPrior;
         this._skycrpgCompassOwned = false;
         this._skycrpgCompassClip = undefined;
      };
      this._skycrpgCompassApply = function(ready) {
         var compass = this.CompassShoutMeterHolder == undefined ? undefined : this.CompassShoutMeterHolder.Compass;
         if (ready != true || compass == undefined) { this._skycrpgCompassRelease(); return; }
         if (this._skycrpgCompassOwned == true && this._skycrpgCompassClip != compass) this._skycrpgCompassRelease();
         if (this._skycrpgCompassOwned != true) {
            this._skycrpgCompassPrior = compass._visible;
            this._skycrpgCompassClip = compass;
            this._skycrpgCompassOwned = true;
         } else if (compass._visible != false) {
            this._skycrpgCompassPrior = compass._visible;
         }
         compass._visible = false;
      };
      var skycrpgWatch = this.createEmptyMovieClip("SkyCRPGCompassWatch", 18003);
      skycrpgWatch.nextSample = 0;
      skycrpgWatch.since = undefined;
      skycrpgWatch.token = undefined;
      skycrpgWatch.onUnload = function() { this._parent._skycrpgCompassRelease(); };
      skycrpgWatch.onEnterFrame = function() {
         var hud = this._parent;
         var bridge = skse != undefined && skse.plugins != undefined ? skse.plugins.SkyCRPGUI : undefined;
         var map = hud.Minimap;
         var visible = map != undefined;
         var ancestor = map;
         while (visible && ancestor != hud) {
            visible = ancestor != undefined && ancestor._visible == true && ancestor._alpha > 0;
            if (visible) ancestor = ancestor._parent;
         }
         visible = visible && hud._visible == true && hud._alpha > 0;
         var bounds = visible && typeof(map.getBounds) == "function" ? map.getBounds(_root) : undefined;
         var inside = bounds != undefined && isFinite(bounds.xMin) && isFinite(bounds.xMax) &&
            isFinite(bounds.yMin) && isFinite(bounds.yMax) && bounds.xMin < bounds.xMax && bounds.yMin < bounds.yMax &&
            bounds.xMin >= Stage.width / 2 && bounds.xMax <= Stage.width - 4 &&
            bounds.yMin >= 4 && bounds.yMax <= Stage.height / 2;
         var mode = hud.CompassShoutMeterHolder != undefined && hud.CheckAgainstHudMode(hud.CompassShoutMeterHolder);
         var blocked = bridge == undefined || typeof(bridge.isMenuFocusBlocked) != "function" || bridge.isMenuFocusBlocked() != false;
         if (!inside || !mode || blocked || typeof(bridge.getMinimapCapability) != "function") {
            this.since = undefined; this.token = undefined; hud._skycrpgCompassRelease(); return;
         }
         var now = getTimer();
         if (now < this.nextSample) return;
         this.nextSample = now + 100;
         var sample = bridge.getMinimapCapability();
         if (sample == undefined || sample.enabled != true || sample.exact != true || sample.gameplay != true ||
             typeof(sample.cellToken) != "number" || !isFinite(sample.cellToken)) {
            this.since = undefined; this.token = undefined; hud._skycrpgCompassRelease(); return;
         }
         if (this.token != sample.cellToken || this.since == undefined) {
            hud._skycrpgCompassRelease(); this.token = sample.cellToken; this.since = now; return;
         }
         hud._skycrpgCompassApply(now - this.since >= 500);
      };
