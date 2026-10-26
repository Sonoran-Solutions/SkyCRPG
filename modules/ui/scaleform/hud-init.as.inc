      // Original diagnostic HUD; child owns its frame callback and dies with HUD.
      var skycrpg = this.createEmptyMovieClip("SkyCRPGStatus", 18000);
      skycrpg.createTextField("label", 1, 0, 0, 420, 48);
      skycrpg._x = 24; skycrpg._y = 90;
      skycrpg.label.selectable = false;
      skycrpg.label.multiline = true;
      skycrpg.onEnterFrame = function() {
         var hud = this._parent;
         var map = hud.Minimap;
         // Attachment/visibility is diagnostic, never proof of renderer health.
         var visible = map != undefined && map._visible && map.MapClip != undefined;
         this.label.text = "SkyCRPG UI spike | H " + Math.round(hud.lastHealthMeterPercent) +
            " M " + Math.round(hud.lastMagickaMeterPercent) + " S " + Math.round(hud.lastStaminaMeterPercent) +
            "\nMap clip: " + (visible ? "attached" : "absent/hidden") + " | Follower: unavailable | Hotkeys: native";
         this.label.setTextFormat(new TextFormat("$EverywhereFont", 15, 0xE9D59C));
      };
