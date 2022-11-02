declare type Optional<T> = T | undefined;
/**
 * UNICODE CHARACTER DATABASE IN XML
 *
 * @link https://unicode.org/reports/tr42/
 */
export declare class Charinfo {
    constructor(json: object);
    /**
     * 4.4.2 Name properties
     * @link https://unicode.org/reports/tr42/#d1e2671
     */
    get name(): Optional<string>;
    /**
    * 4.4.4 Block
    * @link https://unicode.org/reports/tr42/#d1e2768
    */
    get block(): Optional<string>;
    /**
     * 4.4.5 General Category
     * @link https://unicode.org/reports/tr42/#d1e2791
     */
    get generalCategory(): Optional<string>;
    /**
     * 4.4.6 Combining properties
     * @link https://unicode.org/reports/tr42/#d1e2791
     */
    get combining(): Optional<number>;
    /**
     * 4.4.7 Bidirectional properties
     * @link https://unicode.org/reports/tr42/#d1e2841
     */
    get bidiClass(): Optional<string>;
    /**
    * @link https://unicode.org/reports/tr42/#d1e2841
    */
    get bidiMirrored(): Optional<boolean>;
    /**
    * @link https://unicode.org/reports/tr42/#d1e2841
    */
    get bidiMirroredGlyph(): Optional<string>;
    /**
    * @link https://unicode.org/reports/tr42/#d1e2841
    */
    get bidiControl(): Optional<string>;
    /**
    * @link https://unicode.org/reports/tr42/#d1e2841
    */
    get bidiPairdBraketType(): Optional<string>;
    /**
    * @link https://unicode.org/reports/tr42/#d1e2841
    */
    get bidiPairdBraket(): Optional<string>;
    /**
    * @link https://unicode.org/reports/tr42/#d1e2932
    */
    get decompositionType(): Optional<string>;
    /**
    * 4.4.8 Decomposition properties
    * @link https://unicode.org/reports/tr42/#d1e2932
    */
    get decomposition(): Optional<string>;
    /**
    * @link https://unicode.org/reports/tr42/#d1e2932
    */
    get compositionExclusion(): Optional<boolean>;
    /**
    * @link https://unicode.org/reports/tr42/#d1e2932
    */
    get fullCompositionExclusion(): Optional<boolean>;
    /**
    * @link https://unicode.org/reports/tr42/#d1e2993
    */
    get numericType(): Optional<string>;
    /**
    * @link https://unicode.org/reports/tr42/#d1e2993
    */
    get numeric(): number;
    /**
    * @link https://unicode.org/reports/tr42/#d1e3022
    */
    get joiningType(): Optional<string>;
    /**
    * @link https://unicode.org/reports/tr42/#d1e3022
    */
    get joiningGroup(): Optional<string>;
    /**
    * @link https://unicode.org/reports/tr42/#d1e3022
    */
    get joinControl(): Optional<string>;
    /**
     * @link https://unicode.org/reports/tr42/#d1e3067
     */
    get lineBreak(): Optional<string>;
    /**
     * @link https://unicode.org/reports/tr42/#d1e3091
     */
    get eastAsianWidth(): Optional<string>;
    /**
    * @link hhttps://unicode.org/reports/tr42/#d1e3114
    */
    get isUpper(): Optional<boolean>;
    /**
     * @link hhttps://unicode.org/reports/tr42/#d1e3114
     */
    get isLower(): Optional<boolean>;
    /**
     * @link hhttps://unicode.org/reports/tr42/#d1e3114
     */
    get isOtherUpper(): Optional<boolean>;
    /**
     * @link hhttps://unicode.org/reports/tr42/#d1e3114
     */
    get isOtherLower(): Optional<boolean>;
    /**
    * @link hhttps://unicode.org/reports/tr42/#d1e3114
    */
    get upperCase(): Optional<string>;
    /**
    * @link hhttps://unicode.org/reports/tr42/#d1e3114
    */
    get lowerCase(): Optional<string>;
    /**
    * @link hhttps://unicode.org/reports/tr42/#d1e3114
    */
    get titleCase(): Optional<string>;
    /**
    * @link hhttps://unicode.org/reports/tr42/#d1e3114
    */
    get caseFolding(): Optional<string>;
    /**
     * @link https://unicode.org/reports/tr42/#d1e3214
     */
    get script(): Optional<string>;
    /**
     * @link https://unicode.org/reports/tr42/#d1e3241
     */
    get isoComment(): Optional<string>;
    /**
     * @link https://unicode.org/reports/tr42/#d1e3264
     */
    get hangulSyllableType(): Optional<string>;
    /**
     * @link https://unicode.org/reports/tr42/#d1e3302
     */
    get indicSyllabicCategory(): Optional<string>;
    /**
    * @link https://unicode.org/reports/tr42/#d1e3302
    */
    get indicMatraCategory(): Optional<string>;
    /**
    * @link https://unicode.org/reports/tr42/#d1e3302
    */
    get indicPositionalCategory(): Optional<string>;
    /**
     * @link https://unicode.org/reports/tr42/#d1e3357
     */
    get isIDStart(): Optional<boolean>;
    /**
    * @link https://unicode.org/reports/tr42/#d1e3357
    */
    get isOtherIDStart(): Optional<boolean>;
    /**
    * @link https://unicode.org/reports/tr42/#d1e3357
    */
    get isXIDStart(): Optional<boolean>;
    /**
    * @link https://unicode.org/reports/tr42/#d1e3357
    */
    get isIDContinue(): Optional<boolean>;
    /**
    * @link https://unicode.org/reports/tr42/#d1e3357
    */
    get isOtherIDContinue(): Optional<boolean>;
    /**
    * @link https://unicode.org/reports/tr42/#d1e3357
    */
    get isXIDContinue(): Optional<boolean>;
    /**
    * @link https://unicode.org/reports/tr42/#d1e3357
    */
    get isPatternSyntax(): Optional<boolean>;
    /**
    * @link https://unicode.org/reports/tr42/#d1e3357
    */
    get isPatternWhiteSpace(): Optional<boolean>;
    /**
     * @link https://unicode.org/reports/tr42/#d1e3389
     */
    get isDash(): Optional<boolean>;
    /**
     * @link https://unicode.org/reports/tr42/#d1e3389
     */
    get isHyphen(): Optional<boolean>;
    /**
     * @link https://unicode.org/reports/tr42/#d1e3389
     */
    get isQuotationMark(): Optional<boolean>;
    /**
     * @link https://unicode.org/reports/tr42/#d1e3389
     */
    get isTerminal(): Optional<boolean>;
    /**
    * @link https://unicode.org/reports/tr42/#d1e3389
    */
    get isSentenceTerminal(): Optional<boolean>;
    /**
    * @link https://unicode.org/reports/tr42/#d1e3389
    */
    get isDiacritics(): Optional<boolean>;
    /**
    * @link https://unicode.org/reports/tr42/#d1e3389
    */
    get isExtender(): Optional<boolean>;
    /**
    * @link https://unicode.org/reports/tr42/#d1e3389
    */
    get isPCM(): Optional<boolean>;
    /**
    *  @link https://unicode.org/reports/tr42/#d1e3389
    */
    get isSoftDotted(): Optional<boolean>;
    /**
    *  @link https://unicode.org/reports/tr42/#d1e3389
    */
    get isAlphabetic(): Optional<boolean>;
    /**
    *  @link https://unicode.org/reports/tr42/#d1e3389
    */
    get isOtherAlphabetic(): Optional<boolean>;
    /**
    *  @link https://unicode.org/reports/tr42/#d1e3389
    */
    get isMath(): Optional<boolean>;
    /**
     * @link https://unicode.org/reports/tr42/#d1e3389
     */
    get isOtherMath(): Optional<boolean>;
    /**
     * @link https://unicode.org/reports/tr42/#d1e3389
     */
    get isHexDigit(): Optional<boolean>;
    /**
     * @link https://unicode.org/reports/tr42/#d1e3389
     */
    get isOtherHexDigit(): Optional<boolean>;
    /**
     * @link https://unicode.org/reports/tr42/#d1e3389
     */
    get isDefaultIgnorable(): Optional<boolean>;
    /**
     * @link https://unicode.org/reports/tr42/#d1e3389
     */
    get isOtherDefaultIgnorable(): Optional<boolean>;
    /**
     * @link https://unicode.org/reports/tr42/#d1e3389
     */
    get isLogicalOrderException(): Optional<boolean>;
    /**
     * @link https://unicode.org/reports/tr42/#d1e3389
     */
    get isWhitespace(): Optional<boolean>;
    /**
     * @link https://unicode.org/reports/tr42/#d1e3389
     */
    get verticalOrientation(): Optional<string>;
    /**
     * @link https://unicode.org/reports/tr42/#d1e3389
     */
    get isRegionalIndicator(): Optional<boolean>;
    /**
     * @link https://unicode.org/reports/tr42/#d1e3409
     */
    get isGraphemeBase(): Optional<boolean>;
    /**
     * @link https://unicode.org/reports/tr42/#d1e3409
     */
    get isGraphemeExtend(): Optional<boolean>;
    /**
     * @link https://unicode.org/reports/tr42/#d1e3409
     */
    get isGraphemeLink(): Optional<boolean>;
    /**
     * @link https://unicode.org/reports/tr42/#d1e3409
     */
    get graphemeClusterBreak(): Optional<string>;
    /**
     * @link https://unicode.org/reports/tr42/#d1e3409
     */
    get wordBreak(): Optional<string>;
    /**
     * @link https://unicode.org/reports/tr42/#d1e3409
     */
    get sentenceBreak(): Optional<string>;
    /**
     * @link https://unicode.org/reports/tr42/#d1e3430
     */
    get isIdeographic(): Optional<boolean>;
    /**
     * @link https://unicode.org/reports/tr42/#d1e3430
     */
    get isUnifiedIdeograph(): Optional<boolean>;
    /**
     * @link https://unicode.org/reports/tr42/#d1e3430
     */
    get isEquivalentIdeograph(): Optional<boolean>;
    /**
     * @link https://unicode.org/reports/tr42/#d1e3430
     */
    get isIDSBinaryOperator(): Optional<boolean>;
    /**
     * @link https://unicode.org/reports/tr42/#d1e3430
     */
    get isIDSTrinaryOperator(): Optional<boolean>;
    /**
     * @link https://unicode.org/reports/tr42/#d1e3430
     */
    get isRadical(): Optional<boolean>;
    /**
     * @link https://unicode.org/reports/tr42/#d1e3450
     */
    get isDeprecated(): Optional<boolean>;
    /**
     * @link https://unicode.org/reports/tr42/#d1e3450
     */
    get isVariationSelector(): Optional<boolean>;
    /**
     * @link https://unicode.org/reports/tr42/#d1e3450
     */
    get isNoncharacterCodePoint(): Optional<boolean>;
    /**
     * @link https://unicode.org/reports/tr42/#d1e3533
     */
    get isEmoji(): Optional<boolean>;
    /**
     * @link https://unicode.org/reports/tr42/#d1e3533
     */
    get isEmojiPres(): Optional<boolean>;
    /**
    * @link https://unicode.org/reports/tr42/#d1e3533
    */
    get isEmojiMod(): Optional<boolean>;
    /**
     * @link https://unicode.org/reports/tr42/#d1e3533
     */
    get isEmojiBase(): Optional<boolean>;
    /**
     * @link https://unicode.org/reports/tr42/#d1e3533
     */
    get isEmojiComp(): Optional<boolean>;
    /**
     * @link https://unicode.org/reports/tr42/#d1e3533
     */
    get isExtendedPictographic(): Optional<boolean>;
}
export declare class UCD {
    private baseurl;
    constructor(baseurl: string);
    load(key: string): Promise<void>;
    /**
     * @param {number} codepoint the codepoint to get the character for
     */
    charinfo(codepoint: number): Promise<Charinfo>;
}
export {};
