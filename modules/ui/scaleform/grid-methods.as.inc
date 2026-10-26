    // Original SkyCRPG presentation only. The native backing entries and callbacks
    // remain authoritative; no item action is dispatched from these methods.
    public var skycrpgGrid: Boolean = false;

    public function skycrpgMove(details: Object)
    {
        if (!this.skycrpgGrid || this.disableInput || this.disableSelection ||
            !Shared.GlobalFunc.IsKeyPressed(details)) return false;
        var delta = 0;
        if (details.navEquivalent == gfx.ui.NavigationCode.UP) delta = -3;
        else if (details.navEquivalent == gfx.ui.NavigationCode.DOWN) delta = 3;
        else if (details.navEquivalent == gfx.ui.NavigationCode.LEFT) delta = -1;
        else if (details.navEquivalent == gfx.ui.NavigationCode.RIGHT) delta = 1;
        if (delta == 0) return false;
        var n = this.getListEnumSize();
        if (n > 0) {
            var index = this.getSelectedListEnumIndex();
            index = index == undefined || index < 0 ? 0 : Math.max(0, Math.min(n - 1, index + delta));
            this.isMouseDrivenNav = false;
            this.doSetSelectedIndex(this.getListEnumEntry(index).itemIndex, 1);
        }
        return true;
    }

    public function skycrpgPaintCell(clip: MovieClip, entry: Object)
    {
        // Keep each native clip, its itemIndex, mouse listeners and selection path.
        // Replace only its art. No projected/cached item identity is introduced.
        for (var key in clip) {
            var child = clip[key];
            if (child instanceof MovieClip || child instanceof TextField) child._visible = false;
        }
        var selected = entry == this.selectedEntry;
        clip.clear();
        clip.lineStyle(1, selected ? 0xDFC174 : 0x777777, 100);
        clip.beginFill(selected ? 0x48402A : 0x16191D, 95);
        clip.moveTo(0,0); clip.lineTo(165,0); clip.lineTo(165,68);
        clip.lineTo(0,68); clip.lineTo(0,0); clip.endFill();
        if (clip.skycrpgText == undefined) clip.createTextField("skycrpgText", 900, 6, 4, 153, 60);
        var tf = clip.skycrpgText;
        tf._visible = true; tf.selectable = false; tf.multiline = true; tf.wordWrap = true;
        var text = entry.text == undefined ? "?" : String(entry.text);
        var count = entry.count == undefined ? "?" : String(entry.count);
        tf.text = text + "\nCount " + count + "  W " + (entry.infoWeight === undefined ? "?" : (entry.infoWeight == null ? 0 : entry.infoWeight)) +
            (entry.equipState > 0 ? "  [E]" : "");
        tf.setTextFormat(new TextFormat("$EverywhereFont", 13, entry.enabled == false ? 0x888888 : 0xF0E8D8));
    }
