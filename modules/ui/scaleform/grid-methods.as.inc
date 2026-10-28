    // Original SkyCRPG presentation only. The native backing entries and callbacks
    // remain authoritative; no item action is dispatched from these methods.
    public var skycrpgGrid: Boolean = false;
    public var skycrpgCategoryFocus: Boolean = false;
    public var skycrpgObservation: Array;

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
        // Keep native clips, backing indices and listeners. Paint never selects.
        for (var key in clip) {
            var child = clip[key];
            if (child instanceof MovieClip || child instanceof TextField) child._visible = false;
        }
        var selected = entry == this.selectedEntry;
        clip.clear();
        clip.lineStyle(selected ? 2 : 1, selected ? 0xDFC174 : 0x777777, 100);
        clip.beginFill(selected ? 0x48402A : 0x16191D, 98);
        clip.moveTo(0,0); clip.lineTo(165,0); clip.lineTo(165,68);
        clip.lineTo(0,68); clip.lineTo(0,0); clip.endFill();
        if (clip.skycrpgFallback == undefined) clip.createEmptyMovieClip("skycrpgFallback", 899);
        this.skycrpgTypeIcon(clip.skycrpgFallback, entry.formType);
        clip.skycrpgFallback._x = 8; clip.skycrpgFallback._y = 7;
        clip.skycrpgFallback._visible = true;
        // Reuse the native asynchronous icon loader. Reset first so a missing
        // frame label cannot leave the previous item's icon on a recycled cell.
        var icon = clip.itemIcon;
        if (icon != undefined && icon._totalframes > 1 && entry.iconLabel != undefined) {
            icon.gotoAndStop(1); icon.gotoAndStop(entry.iconLabel);
            if (icon._currentframe > 1) {
                icon._x = 8; icon._y = 7; icon._width = 28; icon._height = 28;
                icon._visible = true; clip.skycrpgFallback._visible = false;
            }
        }
        this.skycrpgCellText(clip, "skycrpgName", 900, 40, 5, 119, 36,
            entry.text == undefined ? "Unknown item" : String(entry.text), 13, entry.enabled == false ? 0xAAAAAA : 0xF0E8D8, true);
        this.skycrpgCellText(clip, "skycrpgCount", 901, 6, 40, 47, 22,
            entry.count == undefined ? "x?" : "x" + String(entry.count), 12, 0xDFC174, false);
        this.skycrpgCellText(clip, "skycrpgFlags", 902, 55, 42, 106, 20,
            this.skycrpgStatus(entry, false), 12, 0xF0E8D8, false);
    }

    public function skycrpgCellText(clip: MovieClip, key: String, depth: Number, x: Number, y: Number,
                                   width: Number, height: Number, value: String, size: Number, color: Number, wrap: Boolean)
    {
        if (clip[key] == undefined) clip.createTextField(key, depth, x, y, width, height);
        var tf = clip[key];
        tf._visible = true; tf.selectable = false; tf.autoSize = "none";
        tf.multiline = wrap; tf.wordWrap = wrap;
        tf.setNewTextFormat(new TextFormat("$EverywhereFont", size, color));
        tf.text = value;
        tf.setTextFormat(new TextFormat("$EverywhereFont", size, color));
        if (wrap) {
            // Bounded work even with maliciously long mod names. Full name is
            // retained in the native item card; never rewrite the entry itself.
            tf.text = value.length > 32 ? value.substr(0,29) + "..." : value;
            while (tf.textHeight > height - 4 && tf.text.length > 4) {
                tf.text = tf.text.substr(0, tf.text.length - 4) + "...";
            }
        } else tf.textAutoSize = "shrink";
    }

    public function skycrpgStatus(entry: Object, full: Boolean)
    {
        var row;
        // The DYL-163 observation is cleared before every view/list invalidation.
        // Only use a same-index/count, unambiguous observation for absent fields.
        var rows = this.skycrpgObservation;
        if (rows != undefined) for (var i = 0; i < rows.length; i++) {
            if (rows[i].itemIndex == entry.itemIndex && rows[i].count == entry.count && rows[i].available == true) {
                row = rows[i]; break;
            }
        }
        var equipped = entry.equipState == undefined ? (row == undefined ? undefined : row.worn || row.wornLeft) : entry.equipState > 0;
        var favorite = entry.favorite == undefined ? (row == undefined ? undefined : row.favorite) : entry.favorite;
        var stolen = entry.isStolen == undefined ? (row == undefined ? undefined : row.stolen) : entry.isStolen;
        var quest = entry.isQuestItem == undefined ? (row == undefined ? undefined : row.quest) : entry.isQuestItem;
        return (equipped == undefined ? (full ? "Equipment unknown  " : "E? ") : (equipped ? (full ? "Equipped  " : "E ") : "")) +
            (favorite == undefined ? (full ? "Favorite unknown  " : "F? ") : (favorite ? (full ? "Favorite  " : "F ") : "")) +
            (stolen == undefined ? (full ? "Ownership unknown  " : "S? ") : (stolen ? (full ? "Stolen  " : "S ") : "")) +
            (quest == undefined ? (full ? "Quest status unknown" : "Q?") : (quest ? (full ? "Quest item" : "Q") : ""));
    }

    public function skycrpgTypeIcon(clip: MovieClip, type: Number)
    {
        // Original broad-type vector symbols remain behind unavailable icons.
        clip.clear(); clip.lineStyle(2, 0xDFC174, 100);
        if (type == 41 || type == 42) {
            clip.moveTo(4,26); clip.lineTo(24,3); clip.lineTo(21,13);
            clip.moveTo(6,16); clip.lineTo(14,23);
        } else if (type == 26) {
            clip.moveTo(14,2); clip.lineTo(26,7); clip.lineTo(23,22);
            clip.lineTo(14,28); clip.lineTo(5,22); clip.lineTo(2,7); clip.lineTo(14,2);
        } else if (type == 46 || type == 30 || type == 52) {
            clip.moveTo(9,2); clip.lineTo(19,2); clip.lineTo(19,11);
            clip.lineTo(26,25); clip.lineTo(2,25); clip.lineTo(9,11); clip.lineTo(9,2);
        } else if (type == 27 || type == 23) {
            clip.moveTo(3,4); clip.lineTo(25,4); clip.lineTo(25,26);
            clip.lineTo(3,26); clip.lineTo(3,4); clip.moveTo(8,4); clip.lineTo(8,26);
        } else {
            clip.moveTo(14,2); clip.lineTo(27,14); clip.lineTo(14,27);
            clip.lineTo(1,14); clip.lineTo(14,2);
        }
    }

    // Installed only by the player menu; the shared native list methods and
    // every non-player context retain their original implementation.
    public function skycrpgUpdateScrollPosition(position: Number)
    {
        this._scrollPosition = Math.max(0, Math.min(this._maxScrollPosition, Math.floor(position / 3) * 3));
        this.UpdateList();
    }

    public function skycrpgCalculateMaxScrollPosition()
    {
        this._maxListIndex = 18;
        this._maxScrollPosition = Math.max(0, Math.ceil(this.getListEnumSize() / 3) * 3 - 18);
        this.updateScrollbar();
        if (this._scrollPosition > this._maxScrollPosition) this.scrollPosition = this._maxScrollPosition;
    }

    // Input changes focus; repainting/refreshing a selected cell never does.
    private function onItemRollOver(a_index: Number)
    {
        if (this.skycrpgGrid && !this.isListAnimating && !this.disableInput && !this.disableSelection)
            this.skycrpgCategoryFocus = false;
        super.onItemRollOver(a_index);
    }
