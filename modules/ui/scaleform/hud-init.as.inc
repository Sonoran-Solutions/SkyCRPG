      if (this._skycrpgCompassRelease != undefined) this._skycrpgCompassRelease();
      // Release child frame callbacks and restore only the meter visibility we owned
      // before a repeated initialization. Do not attach another handler on refresh.
      if (this.SkyCRPGStatus != undefined) {
         this.SkyCRPGStatus.onEnterFrame = null;
         if (this.SkyCRPGPartyArea != undefined) this.SkyCRPGPartyArea.onEnterFrame = null;
         if (this.SkyCRPGHotbar != undefined) this.SkyCRPGHotbar.onEnterFrame = null;
         var skycrpgRemoveIndex = this.HudElements.length - 1;
         while (skycrpgRemoveIndex >= 0) {
            if (this.HudElements[skycrpgRemoveIndex] == this.SkyCRPGStatus ||
               this.HudElements[skycrpgRemoveIndex] == this.SkyCRPGPartyArea ||
               this.HudElements[skycrpgRemoveIndex] == this.SkyCRPGHotbar) {
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
      if (this.SkyCRPGHotbar != undefined) this.SkyCRPGHotbar.removeMovieClip();

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
         // Stage anchors are global; HUDMenu has a translated authored origin.
         // Convert both position and a scale basis through its full parent chain.
         skycrpg.placeAtStage = function(stageX, stageY, scalePercent) {
            var origin = {x: stageX, y: stageY};
            var extent = {x: stageX + 100, y: stageY + 100};
            this._parent.globalToLocal(origin);
            this._parent.globalToLocal(extent);
            this._x = origin.x;
            this._y = origin.y;
            this._xscale = (extent.x - origin.x) * scalePercent / 100;
            this._yscale = (extent.y - origin.y) * scalePercent / 100;
         };
         skycrpg.placeAtStage(
            Math.max(16, Math.min(Stage.width - 16 - 420 * skycrpgSettings.hudScale / 100,
               Stage.width / 2 - 210 * skycrpgSettings.hudScale / 100 + skycrpgSettings.hudOffsetX)),
            Math.max(16, Math.min(Stage.height - 12 - 52 * skycrpgSettings.hudScale / 100,
               Stage.height - 64 + skycrpgSettings.hudOffsetY)), skycrpgSettings.hudScale);
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
            skycrpgMeter.createTextField("value", 2, 104, 0, 32, 16);
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
         // Presentation only. Native hotkeys and the native Favorites menu own
         // input and dispatch; this clip has no listeners or synthetic actions.
         var bar = this.createEmptyMovieClip("SkyCRPGHotbar", 18002);
         bar.All = true;
         bar.placeAtStage = skycrpg.placeAtStage;
         bar.placeAtStage(Math.max(16, Math.min(Stage.width - 16 - 536 * skycrpgSettings.hudScale / 100,
            Stage.width / 2 - 268 * skycrpgSettings.hudScale / 100 + skycrpgSettings.hudOffsetX)),
            Math.max(16, Math.min(Stage.height - 16 - 116 * skycrpgSettings.hudScale / 100,
            Stage.height - 122 + skycrpgSettings.hudOffsetY)), skycrpgSettings.hudScale);
         bar._visible = false;
         bar.dataReady = false;
         bar.nextSample = 0;
         bar.createTextField("voice", 20, 4, 61, 528, 16);
         bar.voice.setNewTextFormat(new TextFormat("$EverywhereFont", 11, 0xE8E1D1));
         bar.voice.selectable = false;
         bar.formatRecovery = function(seconds) {
            return typeof(seconds) == "number" && isFinite(seconds) && seconds >= 0 ?
               (Math.ceil(seconds) + "s") : "--";
         };
         bar.syncVisibility = function() {
            var bridge = skse != undefined && skse.plugins != undefined ? skse.plugins.SkyCRPGUI : undefined;
            var blocked = bridge == undefined || typeof(bridge.isMenuFocusBlocked) != "function" ||
               bridge.isMenuFocusBlocked() != false;
            this._visible = this.dataReady && !blocked && this._parent.CheckAgainstHudMode(this);
         };
         bar.onModeChange = function(mode) { this.syncVisibility(); };
         var slotIndex = 0;
         while (slotIndex < 8) {
            var slot = bar.createEmptyMovieClip("slot" + slotIndex, slotIndex + 1);
            slot._x = slotIndex * 67;
            slot.createTextField("key", 1, 4, 2, 58, 15);
            slot.key.setNewTextFormat(new TextFormat("$EverywhereFont", 11, 0xE9D59C, true));
            slot.key.text = String(slotIndex + 1);
            slot.icon = slot.createEmptyMovieClip("icon", 5);
            slot.icon._x = 43;
            slot.icon._y = 2;
            // Original vector symbols identify only broad native form classes;
            // no spell school, weapon subtype or alchemy effect is inferred.
            slot.icon.drawKind = function(kind) {
               this.clear();
               this.lineStyle(1.5, 0xE9D59C, 100);
               if (kind == "WEAPON") {
                  this.moveTo(1, 1); this.lineTo(13, 13);
                  this.moveTo(13, 1); this.lineTo(1, 13);
                  this.moveTo(1, 8); this.lineTo(6, 13);
                  this.moveTo(8, 13); this.lineTo(13, 8);
               } else if (kind == "ARMOR") {
                  this.moveTo(2, 1); this.lineTo(12, 1); this.lineTo(12, 8);
                  this.lineTo(7, 14); this.lineTo(2, 8); this.lineTo(2, 1);
               } else if (kind == "ALCHEMY") {
                  this.moveTo(5, 1); this.lineTo(9, 1); this.lineTo(9, 5);
                  this.lineTo(12, 8); this.lineTo(12, 13); this.lineTo(2, 13);
                  this.lineTo(2, 8); this.lineTo(5, 5); this.lineTo(5, 1);
               } else if (kind == "SPELL") {
                  this.moveTo(7, 0); this.lineTo(9, 5); this.lineTo(14, 7);
                  this.lineTo(9, 9); this.lineTo(7, 14); this.lineTo(5, 9);
                  this.lineTo(0, 7); this.lineTo(5, 5); this.lineTo(7, 0);
               } else if (kind == "SHOUT") {
                  this.moveTo(1, 2); this.lineTo(12, 2); this.lineTo(12, 9);
                  this.lineTo(7, 9); this.lineTo(4, 13); this.lineTo(4, 9);
                  this.lineTo(1, 9); this.lineTo(1, 2);
               } else if (kind == "SCROLL") {
                  this.moveTo(3, 1); this.lineTo(11, 1); this.lineTo(11, 13);
                  this.lineTo(3, 13); this.lineTo(3, 1);
                  this.moveTo(5, 4); this.lineTo(9, 4);
                  this.moveTo(5, 7); this.lineTo(9, 7);
               } else if (kind != "") {
                  this.moveTo(2, 2); this.lineTo(12, 2); this.lineTo(12, 12);
                  this.lineTo(2, 12); this.lineTo(2, 2);
               }
               this.lastKind = kind;
            };
            slot.createTextField("kind", 2, 4, 17, 58, 14);
            slot.kind.setNewTextFormat(new TextFormat("$EverywhereFont", 10, 0xD8C69B, true));
            slot.createTextField("name", 3, 4, 30, 58, 15);
            slot.name.setNewTextFormat(new TextFormat("$EverywhereFont", 10, 0xE8E1D1));
            slot.createTextField("count", 4, 4, 44, 58, 14);
            slot.count.setNewTextFormat(new TextFormat("$EverywhereFont", 10, 0xE8E1D1));
            slot.key.selectable = slot.kind.selectable = slot.name.selectable = slot.count.selectable = false;
            slotIndex++;
         }
         bar.onEnterFrame = function() {
            // Menu transitions must hide immediately even between 2Hz snapshots.
            this.syncVisibility();
            var now = getTimer();
            if (now < this.nextSample) return;
            this.nextSample = now + 500;
            var bridge = skse != undefined && skse.plugins != undefined ? skse.plugins.SkyCRPGUI : undefined;
            var slots = bridge != undefined && typeof(bridge.getHotbar) == "function" ? bridge.getHotbar() : undefined;
            if (slots == undefined || slots.length != 8) {
               this.dataReady = false; this.syncVisibility(); return;
            }
            this.dataReady = true;
            this.syncVisibility();
            this.voice.text = slots.voiceName ? "VOICE: " + slots.voiceName + "  " + this.formatRecovery(slots.voiceRecovery) : "";
            var i = 0;
            while (i < 8) {
               var cell = this["slot" + i];
               var value = slots[i];
               // Only real native hotkey bindings receive a number. FAV is a
               // native favorite selected through the game's Favorites menu.
               cell.key.text = value.kind == "" ? String(i + 1) : (value.bound == true ? String(i + 1) : "FAV");
               cell.clear();
               cell.lineStyle(1, value.equipped ? 0xE9D59C : 0x806E49, 100);
               cell.beginFill(0x171715, 85);
               cell.moveTo(0, 0); cell.lineTo(64, 0); cell.lineTo(64, 59);
               cell.lineTo(0, 59); cell.lineTo(0, 0); cell.endFill();
               cell.icon.drawKind(value.kind);
               cell.kind.text = value.kind == "" ? "EMPTY" : value.kind;
               cell.name.text = value.name;
               // Native voice recovery is global to the player, not a spell timer.
               if (value.kind == "SHOUT") {
                  cell.count.text = this.formatRecovery(value.voiceRecovery);
               } else {
                  cell.count.text = value.count > 1 ? "x" + value.count : "";
               }
               i++;
            }
         };
         this.HudElements.push(bar);
         // SKYCRPG_FOLLOWER_PANEL
         skycrpg._visible = false;
         this.HudElements.push(skycrpg);
         this.HudElements.push(skycrpgParty);
         // Two geometry messages per clip lifetime distinguish missing samples
         // from an offscreen panel without changing layout or adding listeners.
         skycrpg.logGeometry = function(phase, modeVisible) {
            if (skse != undefined && typeof(skse.Log) == "function") {
               var hud = this._parent;
               var globalPanel = {x: 0, y: 0};
               this.localToGlobal(globalPanel);
               skse.Log("SkyCRPG HUD geometry " + phase +
                  " Stage=" + Stage.width + "x" + Stage.height +
                  " parent_x=" + hud._x + " parent_y=" + hud._y +
                  " parent_xscale=" + hud._xscale + " parent_yscale=" + hud._yscale +
                  " panel_x=" + this._x + " panel_y=" + this._y +
                  " panel_xscale=" + this._xscale + " panel_yscale=" + this._yscale +
                  " global_panel_x=" + globalPanel.x + " global_panel_y=" + globalPanel.y +
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
         // The native crosshair/rollover remains visible and owns all target text/actions.
      }
