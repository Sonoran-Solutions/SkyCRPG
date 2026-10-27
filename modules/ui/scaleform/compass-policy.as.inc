      // No supported provider signal establishes functional readiness. Keep native
      // navigation under its existing owner until that contract exists.
      if (this._skycrpgCompassRelease != undefined) this._skycrpgCompassRelease();
      this._skycrpgCompassRelease = function() {
         var compass = this.CompassShoutMeterHolder == undefined ? undefined : this.CompassShoutMeterHolder.Compass;
         if (this._skycrpgCompassOwned == true && compass == this._skycrpgCompassClip && compass._visible == false) {
            compass._visible = this._skycrpgCompassPrior;
         }
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
         }
         compass._visible = false;
      };
      // Readiness is intentionally not wired to clip presence or a guessed API.
      this._skycrpgCompassApply(false);
