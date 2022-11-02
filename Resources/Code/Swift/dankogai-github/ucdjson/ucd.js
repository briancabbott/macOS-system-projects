let __fetch__ = undefined;
if (typeof fetch !== 'function') {
    try {
        __fetch__ = (await import("node-fetch")).default;
    }
    catch (e) { }
}
else {
    __fetch__ = fetch;
}
const loadJSON = typeof __fetch__ === 'function'
    ? async (url) => __fetch__(url).then(r => r.ok ? r.json() : r.status === 404 ? {} : null).catch(e => console.error(e))
    : async (url) => {
        try {
            return require(url);
        }
        catch (e) {
            return {};
        }
    };
const yesno = q => q === 'Y' ? true : q === 'N' ? false : undefined;
/**
 * UNICODE CHARACTER DATABASE IN XML
 *
 * @link https://unicode.org/reports/tr42/
 */
export class Charinfo {
    constructor(json) {
        Object.assign(this, json);
    }
    /**
     * 4.4.2 Name properties
     * @link https://unicode.org/reports/tr42/#d1e2671
     */
    get name() {
        const name = this['na'] || this['na1'];
        return name ? name.replace(/#/, this['cp']) : undefined;
    }
    /**
    * 4.4.4 Block
    * @link https://unicode.org/reports/tr42/#d1e2768
    */
    get block() { return this['blk']; }
    /**
     * 4.4.5 General Category
     * @link https://unicode.org/reports/tr42/#d1e2791
     */
    get generalCategory() { return this['gc']; }
    /**
     * 4.4.6 Combining properties
     * @link https://unicode.org/reports/tr42/#d1e2791
     */
    get combining() { return parseInt(this['ccc']) || undefined; }
    /**
     * 4.4.7 Bidirectional properties
     * @link https://unicode.org/reports/tr42/#d1e2841
     */
    get bidiClass() { return this['bc']; }
    /**
    * @link https://unicode.org/reports/tr42/#d1e2841
    */
    get bidiMirrored() { return yesno(this['Bidi_M']); }
    /**
    * @link https://unicode.org/reports/tr42/#d1e2841
    */
    get bidiMirroredGlyph() { return this['bmg']; }
    /**
    * @link https://unicode.org/reports/tr42/#d1e2841
    */
    get bidiControl() { return this['Bidi_C']; }
    /**
    * @link https://unicode.org/reports/tr42/#d1e2841
    */
    get bidiPairdBraketType() { return this['bpt']; }
    /**
    * @link https://unicode.org/reports/tr42/#d1e2841
    */
    get bidiPairdBraket() { return this['bpb']; }
    /**
    * @link https://unicode.org/reports/tr42/#d1e2932
    */
    get decompositionType() { return this['dt']; }
    /**
    * 4.4.8 Decomposition properties
    * @link https://unicode.org/reports/tr42/#d1e2932
    */
    get decomposition() { return this['dm']; }
    /**
    * @link https://unicode.org/reports/tr42/#d1e2932
    */
    get compositionExclusion() { return yesno(this['CE']); }
    /**
    * @link https://unicode.org/reports/tr42/#d1e2932
    */
    get fullCompositionExclusion() { return yesno(this['Comp_Ex']); }
    /**
    * @link https://unicode.org/reports/tr42/#d1e2993
    */
    get numericType() { return this['nt']; }
    /**
    * @link https://unicode.org/reports/tr42/#d1e2993
    */
    get numeric() { return parseInt(this['nv']); }
    /**
    * @link https://unicode.org/reports/tr42/#d1e3022
    */
    get joiningType() { return this['jt']; }
    /**
    * @link https://unicode.org/reports/tr42/#d1e3022
    */
    get joiningGroup() { return this['jg']; }
    /**
    * @link https://unicode.org/reports/tr42/#d1e3022
    */
    get joinControl() { return this['Join_C']; }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3067
     */
    get lineBreak() { return this['lb']; }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3091
     */
    get eastAsianWidth() { return this['ea']; }
    /**
    * @link hhttps://unicode.org/reports/tr42/#d1e3114
    */
    get isUpper() { return yesno(this['Upper']); }
    /**
     * @link hhttps://unicode.org/reports/tr42/#d1e3114
     */
    get isLower() { return yesno(this['Lower']); }
    /**
     * @link hhttps://unicode.org/reports/tr42/#d1e3114
     */
    get isOtherUpper() { return yesno(this['OUpper']); }
    /**
     * @link hhttps://unicode.org/reports/tr42/#d1e3114
     */
    get isOtherLower() { return yesno(this['OLower']); }
    /**
    * @link hhttps://unicode.org/reports/tr42/#d1e3114
    */
    get upperCase() {
        const u = this['uc'] || this['suc'];
        return u ? u.replace(/#/, this['cp']) : undefined;
    }
    /**
    * @link hhttps://unicode.org/reports/tr42/#d1e3114
    */
    get lowerCase() {
        const l = this['lc'] || this['slc'];
        return l ? l.replace(/#/, this['cp']) : undefined;
    }
    /**
    * @link hhttps://unicode.org/reports/tr42/#d1e3114
    */
    get titleCase() {
        const t = this['tc'] || this['stc'];
        return t ? t.replace(/#/, this['cp']) : undefined;
    }
    /**
    * @link hhttps://unicode.org/reports/tr42/#d1e3114
    */
    get caseFolding() {
        const f = this['cf'] || this['scf'];
        return f ? f.replace(/#/, this['cp']) : undefined;
    }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3214
     */
    get script() { return this['scx'] || this['sc']; }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3241
     */
    get isoComment() { return this['isc']; }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3264
     */
    get hangulSyllableType() { return this['hst']; }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3302
     */
    get indicSyllabicCategory() { return this['InSC']; }
    /**
    * @link https://unicode.org/reports/tr42/#d1e3302
    */
    get indicMatraCategory() { return this['InMC']; }
    /**
    * @link https://unicode.org/reports/tr42/#d1e3302
    */
    get indicPositionalCategory() { return this['InPC']; }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3357
     */
    get isIDStart() { return yesno(this['IDS']); }
    /**
    * @link https://unicode.org/reports/tr42/#d1e3357
    */
    get isOtherIDStart() { return yesno(this['OIDS']); }
    /**
    * @link https://unicode.org/reports/tr42/#d1e3357
    */
    get isXIDStart() { return yesno(this['XIDS']); }
    /**
    * @link https://unicode.org/reports/tr42/#d1e3357
    */
    get isIDContinue() { return yesno(this['IDC']); }
    /**
    * @link https://unicode.org/reports/tr42/#d1e3357
    */
    get isOtherIDContinue() { return yesno(this['OIDC']); }
    /**
    * @link https://unicode.org/reports/tr42/#d1e3357
    */
    get isXIDContinue() { return yesno(this['XIDC']); }
    /**
    * @link https://unicode.org/reports/tr42/#d1e3357
    */
    get isPatternSyntax() { return yesno(this['Pat_Syn']); }
    /**
    * @link https://unicode.org/reports/tr42/#d1e3357
    */
    get isPatternWhiteSpace() { return yesno(this['Pat_WS']); }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3389
     */
    get isDash() { return yesno(this['Dash']); }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3389
     */
    get isHyphen() { return yesno(this['Hyphen']); }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3389
     */
    get isQuotationMark() { return yesno(this['QMark']); }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3389
     */
    get isTerminal() { return yesno(this['Term']); }
    /**
    * @link https://unicode.org/reports/tr42/#d1e3389
    */
    get isSentenceTerminal() { return yesno(this['STerm']); }
    /**
    * @link https://unicode.org/reports/tr42/#d1e3389
    */
    get isDiacritics() { return yesno(this['Dia']); }
    /**
    * @link https://unicode.org/reports/tr42/#d1e3389
    */
    get isExtender() { return yesno(this['Ext']); }
    /**
    * @link https://unicode.org/reports/tr42/#d1e3389
    */
    get isPCM() { return yesno(this['PCM']); }
    /**
    *  @link https://unicode.org/reports/tr42/#d1e3389
    */
    get isSoftDotted() { return yesno(this['SD']); }
    /**
    *  @link https://unicode.org/reports/tr42/#d1e3389
    */
    get isAlphabetic() { return yesno(this['Alpha']); }
    /**
    *  @link https://unicode.org/reports/tr42/#d1e3389
    */
    get isOtherAlphabetic() { return yesno(this['OAlpha']); }
    /**
    *  @link https://unicode.org/reports/tr42/#d1e3389
    */
    get isMath() { return yesno(this['Math']); }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3389
     */
    get isOtherMath() { return yesno(this['OMath']); }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3389
     */
    get isHexDigit() { return yesno(this['Hex']); }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3389
     */
    get isOtherHexDigit() { return yesno(this['OHex']); }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3389
     */
    get isDefaultIgnorable() { return yesno(this['DI']); }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3389
     */
    get isOtherDefaultIgnorable() { return yesno(this['ODI']); }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3389
     */
    get isLogicalOrderException() { return yesno(this['LOE']); }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3389
     */
    get isWhitespace() { return yesno(this['WSpace']); }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3389
     */
    get verticalOrientation() { return this['vo']; }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3389
     */
    get isRegionalIndicator() { return yesno(this['RI']); }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3409
     */
    get isGraphemeBase() { return yesno(this['Gr_Base']); }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3409
     */
    get isGraphemeExtend() { return yesno(this['Gr_Ext']); }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3409
     */
    get isGraphemeLink() { return yesno(this['Gr_Link']); }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3409
     */
    get graphemeClusterBreak() { return this['GCB']; }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3409
     */
    get wordBreak() { return this['WB']; }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3409
     */
    get sentenceBreak() { return this['SB']; }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3430
     */
    get isIdeographic() { return yesno(this['Ideo']); }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3430
     */
    get isUnifiedIdeograph() { return yesno(this['UIdeo']); }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3430
     */
    get isEquivalentIdeograph() { return yesno(this['EIdeo']); }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3430
     */
    get isIDSBinaryOperator() { return yesno(this['IDSB']); }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3430
     */
    get isIDSTrinaryOperator() { return yesno(this['IDST']); }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3430
     */
    get isRadical() { return yesno(this['Radical']); }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3450
     */
    get isDeprecated() { return yesno(this['Dep']); }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3450
     */
    get isVariationSelector() { return yesno(this['VS']); }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3450
     */
    get isNoncharacterCodePoint() { return yesno(this['NChar']); }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3533
     */
    get isEmoji() { return yesno(this['Emoji']); }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3533
     */
    get isEmojiPres() { return yesno(this['Epres']); }
    /**
    * @link https://unicode.org/reports/tr42/#d1e3533
    */
    get isEmojiMod() { return yesno(this['Emod']); }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3533
     */
    get isEmojiBase() { return yesno(this['Ebase']); }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3533
     */
    get isEmojiComp() { return yesno(this['Ecomp']); }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3533
     */
    get isExtendedPictographic() { return yesno(this['ExtPict']); }
}
export class UCD {
    baseurl;
    constructor(baseurl) {
        this.baseurl = baseurl;
    }
    async load(key) {
        const url = `${this.baseurl}/${key}.json`;
        const json = await loadJSON(url);
        this[key] = json;
    }
    /**
     * @param {number} codepoint the codepoint to get the character for
     */
    async charinfo(codepoint) {
        let json = this[codepoint];
        if (!json) {
            let path = codepoint.toString(16).toUpperCase();
            while (path.length < 6) {
                path = '0' + path;
            }
            path = path.replace(/(..)(..)(..)/g, '$1/$2/$3');
            const url = `${this.baseurl}/${path}.json`;
            json = await loadJSON(url);
            if (json)
                this[codepoint] = json;
        }
        return new Charinfo(json);
    }
}
