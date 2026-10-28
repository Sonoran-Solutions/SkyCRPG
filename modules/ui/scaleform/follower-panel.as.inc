         // Original silhouette and copied status only; this panel owns no input.
         var skycrpgParty = this.createEmptyMovieClip("SkyCRPGPartyArea", 18001);
         skycrpgParty.All = true;
         skycrpgParty.placeAtStage = skycrpg.placeAtStage;
         var skycrpgPartyRect = Stage.visibleRect;
         if (skycrpgPartyRect == undefined) skycrpgPartyRect = {x:0, y:0, width:Stage.width, height:Stage.height};
         skycrpgParty.placeAtStage(
            Math.max(skycrpgPartyRect.x + 16, Math.min(skycrpgPartyRect.x + skycrpgPartyRect.width - 16 - 194 * skycrpgSettings.hudScale / 100,
               skycrpgPartyRect.x + 20 + skycrpgSettings.hudOffsetX)),
            Math.max(skycrpgPartyRect.y + 16, Math.min(skycrpgPartyRect.y + skycrpgPartyRect.height - 16 - 49 * skycrpgSettings.hudScale / 100,
               skycrpgPartyRect.y + 62 + skycrpgSettings.hudOffsetY)), skycrpgSettings.hudScale);
         skycrpgParty.lineStyle(1, 0x806E49, 55);
         skycrpgParty.beginFill(0x171715, 45);
         skycrpgParty.moveTo(0, 0); skycrpgParty.lineTo(194, 0); skycrpgParty.lineTo(194, 49);
         skycrpgParty.lineTo(0, 49); skycrpgParty.lineTo(0, 0); skycrpgParty.endFill();
         var portrait = skycrpgParty.createEmptyMovieClip("portrait", 3);
         portrait.beginFill(0x9E9A90, 100);
         portrait.moveTo(18, 7); portrait.lineTo(25, 7); portrait.lineTo(28, 14);
         portrait.lineTo(25, 21); portrait.lineTo(29, 24); portrait.lineTo(34, 39);
         portrait.lineTo(10, 39); portrait.lineTo(15, 24); portrait.lineTo(19, 21);
         portrait.lineTo(16, 14); portrait.lineTo(18, 7); portrait.endFill();
         skycrpgParty.createTextField("heading", 1, 42, 5, 145, 18);
         skycrpgParty.heading.selectable = false;
         skycrpgParty.heading.setNewTextFormat(new TextFormat("$EverywhereFont", 13, 0xD8C69B, true));
         skycrpgParty.createTextField("availability", 2, 42, 25, 145, 18);
         skycrpgParty.availability.selectable = false;
         skycrpgParty.availability.setNewTextFormat(new TextFormat("$EverywhereFont", 11, 0xE8E1D1));
         skycrpgParty.heading.textAutoSize = "shrink";
         skycrpgParty.availability.textAutoSize = "shrink";
         skycrpgParty.nextSample = 0;
         skycrpgParty.state = "unknown";
         skycrpgParty.syncVisibility = function() {
            this._visible = this.state != "none" && this.state != "dead" && this._parent.CheckAgainstHudMode(this);
         };
         skycrpgParty.onModeChange = function(mode) { this.syncVisibility(); };
         skycrpgParty.onEnterFrame = function() {
            this.syncVisibility();
            var now = getTimer();
            if (now < this.nextSample) return;
            this.nextSample = now + 100;
            var bridge = skse != undefined && skse.plugins != undefined ? skse.plugins.SkyCRPGUI : undefined;
            var sample = bridge != undefined && typeof(bridge.getFollowerStatus) == "function" ? bridge.getFollowerStatus() : undefined;
            this.state = sample != undefined ? sample.state : "unknown";
            this.heading.text = sample != undefined && sample.name ? String(sample.name).substr(0, 22) : "PARTY STATUS";
            if (sample != undefined && sample.name && String(sample.name).length > 22)
               this.heading.text = String(sample.name).substr(0,19) + "...";
            if (this.state == "live" && typeof(sample.health) == "number" && isFinite(sample.health)) {
               this.availability.text = "HEALTH " + Math.round(Math.max(0, Math.min(1, sample.health)) * 100) + "%";
            } else if (this.state == "unsupported") {
               this.availability.text = "Followers untested";
            } else {
               this.availability.text = "Follower data unavailable";
            }
            this.syncVisibility();
         };
         skycrpgParty._visible = false;
