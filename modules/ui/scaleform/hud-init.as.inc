      // Release child frame callbacks and restore only the meter visibility we owned
      // before a repeated initialization. Do not attach another handler on refresh.
      if (this.SkyCRPGStatus != undefined) {
         this.SkyCRPGStatus.onEnterFrame = null;
         var skycrpgRemoveIndex = this.HudElements.length - 1;
         while (skycrpgRemoveIndex >= 0) {
            if (this.HudElements[skycrpgRemoveIndex] == this.SkyCRPGStatus ||
               this.HudElements[skycrpgRemoveIndex] == this.SkyCRPGPartyArea) {
               this.HudElements.splice(skycrpgRemoveIndex, 1);
            }
            skycrpgRemoveIndex--;
         }
         if (this._skycrpgMetersSuppressed == true) {
            this.Health._visible = this.CheckAgainstHudMode(this.Health);
            this.Magica._visible = this.CheckAgainstHudMode(this.Magica);
            this.Stamina._visible = this.CheckAgainstHudMode(this.Stamina);
            this._skycrpgMetersSuppressed = false;
         }
         this.SkyCRPGStatus.removeMovieClip();
      }
      if (this.SkyCRPGPartyArea != undefined) this.SkyCRPGPartyArea.removeMovieClip();

      // Read-only config is published by the UI DLL at SKSE plugin initialization.
      // If it is absent/disabled, leave every native meter and prompt untouched.
      var skycrpgSettings = skse != undefined && skse.plugins != undefined ? skse.plugins.SkyCRPGUI : undefined;
      if (skycrpgSettings != undefined && skycrpgSettings.hudEnabled == true) {
         // Fresh current/effective maximum snapshots avoid the native callback cache,
         // whose initialized zeroes are not authoritative player data.
         // This child owns no global listeners and is destroyed with this HUDMenu.
         var skycrpg = this.createEmptyMovieClip("SkyCRPGStatus", 18000);
         skycrpg.createEmptyMovieClip("meters", 1);
         skycrpg.All = true;
         skycrpg._xscale = skycrpgSettings.hudScale;
         skycrpg._yscale = skycrpgSettings.hudScale;
         skycrpg._x = Math.max(16, Math.min(Stage.width - 16 - 420 * skycrpgSettings.hudScale / 100,
            Stage.width / 2 - 210 * skycrpgSettings.hudScale / 100 + skycrpgSettings.hudOffsetX));
         skycrpg._y = Math.max(16, Math.min(Stage.height - 12 - 52 * skycrpgSettings.hudScale / 100,
            Stage.height - 64 + skycrpgSettings.hudOffsetY));
         skycrpg._visible = false;
         skycrpg._skycrpgDataReady = false;
         skycrpg._skycrpgNextSample = 0;
         skycrpg.createTextField("title", 10, 0, 0, 420, 20);
         skycrpg.title.selectable = false;
         skycrpg.title.text = "ADVENTURER STATUS";
         skycrpg.title.setTextFormat(new TextFormat("$EverywhereFont", 14, 0xE9D59C, true));
         skycrpg.meters._y = 21;
         var skycrpgLabels = ["HEALTH", "MAGICKA", "STAMINA"];
         var skycrpgColors = [0xA84C42, 0x6384A1, 0x9B9A61];
         var skycrpgIndex = 0;
         while (skycrpgIndex < 3) {
            var skycrpgMeter = skycrpg.meters.createEmptyMovieClip("meter" + skycrpgIndex, skycrpgIndex + 1);
            skycrpgMeter._x = skycrpgIndex * 142;
            skycrpgMeter.createTextField("label", 1, 0, 0, 136, 16);
            skycrpgMeter.label.selectable = false;
            skycrpgMeter.label.text = skycrpgLabels[skycrpgIndex];
            skycrpgMeter.label.setTextFormat(new TextFormat("$EverywhereFont", 12, 0xD8C69B, true));
            skycrpgMeter.createTextField("value", 2, 112, 0, 32, 16);
            skycrpgMeter.value.selectable = false;
            skycrpgMeter.value.autoSize = "right";
            // drawPercent assigns text later, so use the future-text format.
            skycrpgMeter.value.setNewTextFormat(new TextFormat("$EverywhereFont", 12, 0xE8E1D1, true));
            skycrpgMeter.bar = skycrpgMeter.createEmptyMovieClip("bar", 3);
            skycrpgMeter.bar._y = 18;
            skycrpgMeter.barColor = skycrpgColors[skycrpgIndex];
            skycrpgMeter._skycrpgValue = -1;
            skycrpgMeter.drawPercent = function(percent) {
               var bounded = Math.max(0, Math.min(100, Number(percent)));
               if (isNaN(bounded)) return;
               if (this._skycrpgValue == bounded) return;
               this._skycrpgValue = bounded;
               this.value.text = Math.round(bounded) + "%";
               this.bar.clear();
               this.bar.lineStyle(1, 0x806E49, 100);
               this.bar.beginFill(0x171715, 95);
               this.bar.moveTo(0, 0); this.bar.lineTo(136, 0); this.bar.lineTo(136, 11);
               this.bar.lineTo(0, 11); this.bar.lineTo(0, 0); this.bar.endFill();
               if (bounded > 0) {
                  this.bar.beginFill(this.barColor, 100);
                  this.bar.moveTo(2, 2); this.bar.lineTo(2 + 1.32 * bounded, 2);
                  this.bar.lineTo(2 + 1.32 * bounded, 9); this.bar.lineTo(2, 9);
                  this.bar.lineTo(2, 2); this.bar.endFill();
               }
            };
            skycrpgIndex++;
         }
         var skycrpgParty = this.createEmptyMovieClip("SkyCRPGPartyArea", 18001);
         skycrpgParty.All = true;
         skycrpgParty._x = Math.max(16, Math.min(Stage.width - 16 - 194 * skycrpgSettings.hudScale / 100,
            20 + skycrpgSettings.hudOffsetX));
         skycrpgParty._y = Math.max(16, Math.min(Stage.height - 16 - 49 * skycrpgSettings.hudScale / 100,
            62 + skycrpgSettings.hudOffsetY));
         skycrpgParty._xscale = skycrpgSettings.hudScale;
         skycrpgParty._yscale = skycrpgSettings.hudScale;
         skycrpgParty.lineStyle(1, 0x806E49, 55);
         skycrpgParty.beginFill(0x171715, 45);
         skycrpgParty.moveTo(0, 0); skycrpgParty.lineTo(194, 0); skycrpgParty.lineTo(194, 49);
         skycrpgParty.lineTo(0, 49); skycrpgParty.lineTo(0, 0); skycrpgParty.endFill();
         skycrpgParty.createTextField("heading", 1, 10, 5, 174, 18);
         skycrpgParty.heading.selectable = false;
         skycrpgParty.heading.text = "PARTY STATUS";
         skycrpgParty.heading.setTextFormat(new TextFormat("$EverywhereFont", 13, 0xD8C69B, true));
         skycrpgParty.createTextField("availability", 2, 10, 25, 174, 18);
         skycrpgParty.availability.selectable = false;
         skycrpgParty.availability.text = "Follower data unavailable";
         skycrpgParty.availability.setTextFormat(new TextFormat("$EverywhereFont", 11, 0x9E9A90));
         skycrpg._visible = false;
         skycrpgParty._visible = this.CheckAgainstHudMode(skycrpgParty);
         this.HudElements.push(skycrpg);
         this.HudElements.push(skycrpgParty);
         // Two geometry messages per clip lifetime distinguish missing samples
         // from an offscreen panel without changing layout or adding listeners.
         skycrpg.logGeometry = function(phase, modeVisible) {
            if (skse != undefined && typeof(skse.Log) == "function") {
               var hud = this._parent;
               skse.Log("SkyCRPG HUD geometry " + phase +
                  " Stage=" + Stage.width + "x" + Stage.height +
                  " parent_x=" + hud._x + " parent_y=" + hud._y +
                  " parent_xscale=" + hud._xscale + " parent_yscale=" + hud._yscale +
                  " panel_x=" + this._x + " panel_y=" + this._y +
                  " panel_xscale=" + this._xscale + " panel_yscale=" + this._yscale +
                  " modeVisible=" + modeVisible);
            }
         };
         skycrpg.logGeometry("setup", this.CheckAgainstHudMode(skycrpg));
         skycrpg._skycrpgRenderedLogged = false;
         skycrpg.onEnterFrame = function() {
            var hud = this._parent;
            var now = getTimer();
            if (now < this._skycrpgNextSample) return;
            this._skycrpgNextSample = now + 100; // At most 10 samples/sec; no catch-up loop.
            var bridge = skse != undefined && skse.plugins != undefined ? skse.plugins.SkyCRPGUI : undefined;
            var sample = bridge != undefined && bridge.hudEnabled == true &&
               typeof(bridge.getPlayerResources) == "function" ? bridge.getPlayerResources() : undefined;
            var ready = sample != undefined;
            var names = ["health", "magicka", "stamina"];
            var values = [];
            var i = 0;
            while (ready && i < 3) {
               var current = sample[names[i]];
               var maximum = sample[names[i] + "Max"];
               ready = typeof(current) == "number" && typeof(maximum) == "number" &&
                  !isNaN(current) && !isNaN(maximum) && isFinite(current) && isFinite(maximum) && maximum > 0;
               values[i] = 100 * current / maximum;
               ready = ready && isFinite(values[i]);
               i++;
            }
            var modeVisible = hud.CheckAgainstHudMode(this);
            if (!ready) {
               this._visible = false;
               this._skycrpgDataReady = false;
               hud.Health._visible = hud.CheckAgainstHudMode(hud.Health);
               hud.Magica._visible = hud.CheckAgainstHudMode(hud.Magica);
               hud.Stamina._visible = hud.CheckAgainstHudMode(hud.Stamina);
               hud._skycrpgMetersSuppressed = false;
               return;
            }
            this._skycrpgDataReady = true;
            this._visible = modeVisible;
            this.meters.meter0.drawPercent(values[0]);
            this.meters.meter1.drawPercent(values[1]);
            this.meters.meter2.drawPercent(values[2]);
            if (modeVisible && !this._skycrpgRenderedLogged) {
               this.logGeometry("first-valid-render", modeVisible);
               this._skycrpgRenderedLogged = true;
            }
            // Suppress duplicates only while the replacement is allowed by the
            // native mode and the fresh complete snapshot is valid.
            hud._skycrpgMetersSuppressed = modeVisible;
            hud.Health._visible = modeVisible ? false : hud.CheckAgainstHudMode(hud.Health);
            hud.Magica._visible = modeVisible ? false : hud.CheckAgainstHudMode(hud.Magica);
            hud.Stamina._visible = modeVisible ? false : hud.CheckAgainstHudMode(hud.Stamina);
         };
         skycrpgParty.onModeChange = function(mode) {
            this._visible = this.hasOwnProperty(mode);
         };
         // The native crosshair/rollover remains visible and owns all target text/actions.
      }
