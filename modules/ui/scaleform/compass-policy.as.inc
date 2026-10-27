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
      // Log each lease state change once, capped per clip lifetime, so a kept
      // compass names its reason in skse64.log without per-frame output.
      skycrpgWatch.reported = undefined;
      skycrpgWatch.reportCount = 0;
      skycrpgWatch.report = function(reason, bounds) {
         if (reason == this.reported || this.reportCount >= 32) return;
         this.reported = reason; this.reportCount++;
         if (skse == undefined || typeof(skse.Log) != "function") return;
         skse.Log("SkyCRPG compass lease " + reason + " Stage=" + Stage.width + "x" + Stage.height +
            (bounds == undefined ? "" : " map=" + bounds.xMin + "," + bounds.yMin + "," + bounds.xMax + "," + bounds.yMax));
      };
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
            isFinite(bounds.yMin) && isFinite(bounds.yMax) && bounds.xMin < bounds.xMax && bounds.yMin < bounds.yMax;
         if (inside) {
            // DEM's frame sits flush to the corner and its quest pointer/distance
            // label overhang the screen edge, so whole-clip containment is wrong.
            // Require the clip's centre in the upper-right quadrant and at least
            // half of its area on screen.
            var centreX = (bounds.xMin + bounds.xMax) / 2;
            var centreY = (bounds.yMin + bounds.yMax) / 2;
            var shownWidth = Math.min(bounds.xMax, Stage.width) - Math.max(bounds.xMin, 0);
            var shownHeight = Math.min(bounds.yMax, Stage.height) - Math.max(bounds.yMin, 0);
            inside = centreX >= Stage.width / 2 && centreX <= Stage.width && centreY >= 0 && centreY <= Stage.height / 2 &&
               shownWidth > 0 && shownHeight > 0 &&
               shownWidth * shownHeight >= 0.5 * (bounds.xMax - bounds.xMin) * (bounds.yMax - bounds.yMin);
         }
         var mode = hud.CompassShoutMeterHolder != undefined && hud.CheckAgainstHudMode(hud.CompassShoutMeterHolder);
         var blocked = bridge == undefined || typeof(bridge.isMenuFocusBlocked) != "function" || bridge.isMenuFocusBlocked() != false;
         var reason = map == undefined ? "no-map" : !visible ? "hidden" : !inside ? "bounds" : !mode ? "hud-mode" :
            blocked ? "menu" : typeof(bridge.getMinimapCapability) != "function" ? "no-bridge" : undefined;
         if (reason != undefined) {
            this.report(reason, bounds);
            this.since = undefined; this.token = undefined; hud._skycrpgCompassRelease(); return;
         }
         var now = getTimer();
         if (now < this.nextSample) return;
         this.nextSample = now + 100;
         var sample = bridge.getMinimapCapability();
         if (sample == undefined || sample.enabled != true || sample.exact != true || sample.gameplay != true ||
             typeof(sample.cellToken) != "number" || !isFinite(sample.cellToken)) {
            this.report(sample == undefined ? "stale-sample" : sample.enabled != true ? "disabled" :
               sample.exact != true ? "inexact-install" : sample.gameplay != true ? "not-gameplay" : "cell-token", bounds);
            this.since = undefined; this.token = undefined; hud._skycrpgCompassRelease(); return;
         }
         if (this.token != sample.cellToken || this.since == undefined) {
            this.report("settling", bounds);
            hud._skycrpgCompassRelease(); this.token = sample.cellToken; this.since = now; return;
         }
         var settled = now - this.since >= 500;
         if (settled) this.report("suppressed", bounds);
         hud._skycrpgCompassApply(settled);
      };
