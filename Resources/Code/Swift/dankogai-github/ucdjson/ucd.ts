let __fetch__ = undefined;
if (typeof fetch !== 'function') {
    try {
        __fetch__ = (await import("node-fetch")).default;
    } catch (e) {}
} else {
    __fetch__ = fetch;
}

const loadJSON: (url: string) => Promise<any> = typeof __fetch__ === 'function'
    ? async (url) => __fetch__(url).then(
        r => r.ok ? r.json() : r.status === 404 ? {} : null
    ).catch(e => console.error(e))
    : async (url) => {
        try {
            return require(url);
        } catch (e) {
            return {};
        }
    };

type Optional<T> = T | undefined; // to make vscode happy

const yesno: (q: string) => Optional<boolean> = q => q === 'Y' ? true : q === 'N' ? false : undefined;

/**
 * UNICODE CHARACTER DATABASE IN XML
 * 
 * @link https://unicode.org/reports/tr42/
 */
export class Charinfo {
    constructor(json: object) {
        Object.assign(this, json);
    }
    /**
     * 4.4.2 Name properties
     * @link https://unicode.org/reports/tr42/#d1e2671
     */
    get name(): Optional<string> {
        const name = this['na'] || this['na1'];
        return name ? name.replace(/#/, this['cp']) : undefined;
    }
    /**
    * 4.4.4 Block
    * @link https://unicode.org/reports/tr42/#d1e2768 
    */
    get block(): Optional<string> { return this['blk'] }
    /**
     * 4.4.5 General Category
     * @link https://unicode.org/reports/tr42/#d1e2791
     */
    get generalCategory(): Optional<string> { return this['gc'] }
    /**
     * 4.4.6 Combining properties
     * @link https://unicode.org/reports/tr42/#d1e2791
     */
    get combining(): Optional<number> { return parseInt(this['ccc']) || undefined }
    /**
     * 4.4.7 Bidirectional properties
     * @link https://unicode.org/reports/tr42/#d1e2841
     */
    get bidiClass(): Optional<string> { return this['bc'] }
    /**
    * @link https://unicode.org/reports/tr42/#d1e2841
    */
    get bidiMirrored(): Optional<boolean> { return yesno(this['Bidi_M']) }
    /**
    * @link https://unicode.org/reports/tr42/#d1e2841
    */
    get bidiMirroredGlyph(): Optional<string> { return this['bmg'] }
    /**
    * @link https://unicode.org/reports/tr42/#d1e2841
    */
    get bidiControl(): Optional<string> { return this['Bidi_C'] }
    /**
    * @link https://unicode.org/reports/tr42/#d1e2841
    */
    get bidiPairdBraketType(): Optional<string> { return this['bpt'] }
    /**
    * @link https://unicode.org/reports/tr42/#d1e2841
    */
    get bidiPairdBraket(): Optional<string> { return this['bpb'] }
    /**
    * @link https://unicode.org/reports/tr42/#d1e2932
    */
    get decompositionType(): Optional<string> { return this['dt'] }
    /**
    * 4.4.8 Decomposition properties
    * @link https://unicode.org/reports/tr42/#d1e2932
    */
    get decomposition(): Optional<string> { return this['dm'] }
    /**
    * @link https://unicode.org/reports/tr42/#d1e2932
    */
    get compositionExclusion(): Optional<boolean> { return yesno(this['CE']) }
    /**
    * @link https://unicode.org/reports/tr42/#d1e2932
    */
    get fullCompositionExclusion(): Optional<boolean> { return yesno(this['Comp_Ex']) }
    /**
    * @link https://unicode.org/reports/tr42/#d1e2993
    */
    get numericType(): Optional<string> { return this['nt'] }
    /**
    * @link https://unicode.org/reports/tr42/#d1e2993
    */
    get numeric(): number { return parseInt(this['nv']) }
    /**
    * @link https://unicode.org/reports/tr42/#d1e3022
    */
    get joiningType(): Optional<string> { return this['jt'] }
    /**
    * @link https://unicode.org/reports/tr42/#d1e3022
    */
    get joiningGroup(): Optional<string> { return this['jg'] }
    /**
    * @link https://unicode.org/reports/tr42/#d1e3022
    */
    get joinControl(): Optional<string> { return this['Join_C'] }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3067
     */
    get lineBreak(): Optional<string> { return this['lb'] }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3091
     */
    get eastAsianWidth(): Optional<string> { return this['ea'] }
    /**
    * @link hhttps://unicode.org/reports/tr42/#d1e3114
    */
    get isUpper(): Optional<boolean> { return yesno(this['Upper']) }
    /**
     * @link hhttps://unicode.org/reports/tr42/#d1e3114
     */
    get isLower(): Optional<boolean> { return yesno(this['Lower']) }
    /**
     * @link hhttps://unicode.org/reports/tr42/#d1e3114
     */
    get isOtherUpper(): Optional<boolean> { return yesno(this['OUpper']) }
    /**
     * @link hhttps://unicode.org/reports/tr42/#d1e3114
     */
    get isOtherLower(): Optional<boolean> { return yesno(this['OLower']) }
    /**
    * @link hhttps://unicode.org/reports/tr42/#d1e3114
    */
    get upperCase(): Optional<string> {
        const u = this['uc'] || this['suc'];
        return u ? u.replace(/#/, this['cp']) : undefined;
    }
    /**
    * @link hhttps://unicode.org/reports/tr42/#d1e3114
    */
    get lowerCase(): Optional<string> {
        const l = this['lc'] || this['slc'];
        return l ? l.replace(/#/, this['cp']) : undefined;
    }
    /**
    * @link hhttps://unicode.org/reports/tr42/#d1e3114
    */
    get titleCase(): Optional<string> {
        const t = this['tc'] || this['stc'];
        return t ? t.replace(/#/, this['cp']) : undefined;
    }
    /**
    * @link hhttps://unicode.org/reports/tr42/#d1e3114
    */
    get caseFolding(): Optional<string> {
        const f = this['cf'] || this['scf'];
        return f ? f.replace(/#/, this['cp']) : undefined;
    }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3214
     */
    get script(): Optional<string> { return this['scx'] || this['sc'] }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3241
     */
    get isoComment(): Optional<string> { return this['isc'] }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3264
     */
    get hangulSyllableType(): Optional<string> { return this['hst'] }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3302
     */
    get indicSyllabicCategory(): Optional<string> { return this['InSC'] }
    /**
    * @link https://unicode.org/reports/tr42/#d1e3302
    */
    get indicMatraCategory(): Optional<string> { return this['InMC'] }
    /**
    * @link https://unicode.org/reports/tr42/#d1e3302
    */
    get indicPositionalCategory(): Optional<string> { return this['InPC'] }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3357
     */
    get isIDStart(): Optional<boolean> { return yesno(this['IDS']) }
    /**
    * @link https://unicode.org/reports/tr42/#d1e3357
    */
    get isOtherIDStart(): Optional<boolean> { return yesno(this['OIDS']) }
    /**
    * @link https://unicode.org/reports/tr42/#d1e3357
    */
    get isXIDStart(): Optional<boolean> { return yesno(this['XIDS']) }
    /**
    * @link https://unicode.org/reports/tr42/#d1e3357
    */
    get isIDContinue(): Optional<boolean> { return yesno(this['IDC']) }
    /**
    * @link https://unicode.org/reports/tr42/#d1e3357
    */
    get isOtherIDContinue(): Optional<boolean> { return yesno(this['OIDC']) }
    /**
    * @link https://unicode.org/reports/tr42/#d1e3357
    */
    get isXIDContinue(): Optional<boolean> { return yesno(this['XIDC']) }
    /**
    * @link https://unicode.org/reports/tr42/#d1e3357
    */
    get isPatternSyntax(): Optional<boolean> { return yesno(this['Pat_Syn']) }
    /**
    * @link https://unicode.org/reports/tr42/#d1e3357
    */
    get isPatternWhiteSpace(): Optional<boolean> { return yesno(this['Pat_WS']) }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3389
     */
    get isDash(): Optional<boolean> { return yesno(this['Dash']) }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3389
     */
    get isHyphen(): Optional<boolean> { return yesno(this['Hyphen']) }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3389
     */
    get isQuotationMark(): Optional<boolean> { return yesno(this['QMark']) }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3389
     */
    get isTerminal(): Optional<boolean> { return yesno(this['Term']) }
    /**
    * @link https://unicode.org/reports/tr42/#d1e3389
    */
    get isSentenceTerminal(): Optional<boolean> { return yesno(this['STerm']) }
    /**
    * @link https://unicode.org/reports/tr42/#d1e3389
    */
    get isDiacritics(): Optional<boolean> { return yesno(this['Dia']) }
    /**
    * @link https://unicode.org/reports/tr42/#d1e3389
    */
    get isExtender(): Optional<boolean> { return yesno(this['Ext']) }
    /**
    * @link https://unicode.org/reports/tr42/#d1e3389
    */
    get isPCM(): Optional<boolean> { return yesno(this['PCM']) }
    /**
    *  @link https://unicode.org/reports/tr42/#d1e3389
    */
    get isSoftDotted(): Optional<boolean> { return yesno(this['SD']) }
    /**
    *  @link https://unicode.org/reports/tr42/#d1e3389
    */
    get isAlphabetic(): Optional<boolean> { return yesno(this['Alpha']) }
    /**
    *  @link https://unicode.org/reports/tr42/#d1e3389
    */
    get isOtherAlphabetic(): Optional<boolean> { return yesno(this['OAlpha']) }
    /**
    *  @link https://unicode.org/reports/tr42/#d1e3389
    */
    get isMath(): Optional<boolean> { return yesno(this['Math']) }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3389
     */
    get isOtherMath(): Optional<boolean> { return yesno(this['OMath']) }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3389
     */
    get isHexDigit(): Optional<boolean> { return yesno(this['Hex']) }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3389
     */
    get isOtherHexDigit(): Optional<boolean> { return yesno(this['OHex']) }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3389
     */
    get isDefaultIgnorable(): Optional<boolean> { return yesno(this['DI']) }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3389
     */
    get isOtherDefaultIgnorable(): Optional<boolean> { return yesno(this['ODI']) }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3389
     */
    get isLogicalOrderException(): Optional<boolean> { return yesno(this['LOE']) }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3389
     */
    get isWhitespace(): Optional<boolean> { return yesno(this['WSpace']) }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3389
     */
    get verticalOrientation(): Optional<string> { return this['vo'] }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3389
     */
    get isRegionalIndicator(): Optional<boolean> { return yesno(this['RI']) }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3409
     */
    get isGraphemeBase(): Optional<boolean> { return yesno(this['Gr_Base']) }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3409
     */
    get isGraphemeExtend(): Optional<boolean> { return yesno(this['Gr_Ext']) }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3409
     */
    get isGraphemeLink(): Optional<boolean> { return yesno(this['Gr_Link']) }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3409
     */
    get graphemeClusterBreak(): Optional<string> { return this['GCB'] }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3409
     */
    get wordBreak(): Optional<string> { return this['WB'] }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3409
     */
    get sentenceBreak(): Optional<string> { return this['SB'] }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3430
     */
    get isIdeographic(): Optional<boolean> { return yesno(this['Ideo']) }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3430
     */
    get isUnifiedIdeograph(): Optional<boolean> { return yesno(this['UIdeo']) }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3430
     */
    get isEquivalentIdeograph(): Optional<boolean> { return yesno(this['EIdeo']) }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3430
     */
    get isIDSBinaryOperator(): Optional<boolean> { return yesno(this['IDSB']) }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3430
     */
    get isIDSTrinaryOperator(): Optional<boolean> { return yesno(this['IDST']) }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3430
     */
    get isRadical(): Optional<boolean> { return yesno(this['Radical']) }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3450
     */
    get isDeprecated(): Optional<boolean> { return yesno(this['Dep']) }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3450
     */
    get isVariationSelector(): Optional<boolean> { return yesno(this['VS']) }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3450
     */
    get isNoncharacterCodePoint(): Optional<boolean> { return yesno(this['NChar']) }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3533
     */
    get isEmoji(): Optional<boolean> { return yesno(this['Emoji']) }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3533
     */
    get isEmojiPres(): Optional<boolean> { return yesno(this['Epres']) }
    /**
    * @link https://unicode.org/reports/tr42/#d1e3533
    */
    get isEmojiMod(): Optional<boolean> { return yesno(this['Emod']) }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3533
     */
    get isEmojiBase(): Optional<boolean> { return yesno(this['Ebase']) }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3533
     */
    get isEmojiComp(): Optional<boolean> { return yesno(this['Ecomp']) }
    /**
     * @link https://unicode.org/reports/tr42/#d1e3533
     */
    get isExtendedPictographic(): Optional<boolean> { return yesno(this['ExtPict']) }
}

export class UCD {
    private baseurl: string;
    constructor(baseurl: string) {
        this.baseurl = baseurl;
    }
    async load(key: string) {
        const url = `${this.baseurl}/${key}.json`;
        const json = await loadJSON(url);
        this[key] = json;
    }
    /**
     * @param {number} codepoint the codepoint to get the character for
     */
    async charinfo(codepoint: number): Promise<Charinfo> {
        let json = this[codepoint];
        if (!json) {
            let path = codepoint.toString(16).toUpperCase();
            while (path.length < 6) { path = '0' + path; }
            path = path.replace(/(..)(..)(..)/g, '$1/$2/$3');
            const url = `${this.baseurl}/${path}.json`;
            json = await loadJSON(url);
            if (json) this[codepoint] = json;
        }
        return new Charinfo(json);
    }
}
