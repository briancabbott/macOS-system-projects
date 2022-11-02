/**
 *  Licensed under the MIT license.
 *  http://www.opensource.org/licenses/mit-license.php
 *
 *  @author: Dan Kogai <dankogai+github@gmail.com>
 *
 *  References:
 *  @link: https://dankogai.github.io/SION/
*/
export declare const version = "1.2.0";
/**
 * A regular expression that matches a hexadecimal floating-point notation
 */
export declare const RE_HEXFLOAT: RegExp;
/**
 * Parse a hexadecimal floating-point notation in `string` to `number`.
 */
export declare const RE_HEXFLOAT_G: RegExp;
/**
 * Parse a hexadecimal floating-point notation in `string` to `number`.
 */
export declare const parseHexFloat: (str: string) => number;
/**
 * Stringify a `number` to a hexadecimal floating-point notation
 */
export declare const toHexString: (num: number) => string;
/**
 * Stringify a given object to a `SION` string
 */
export declare const stringify: (obj: any, replacer?: (any: any) => any, space?: (number | string), depth?: number) => string;
/**
 * Parses a `SION` string to a JS object
 */
export declare const parse: (str: string) => any;
/**
 * A namespace
 */
export declare const SION: {
    version: string;
    RE_HEXFLOAT: RegExp;
    RE_HEXFLOAT_G: RegExp;
    parseHexFloat: (str: string) => number;
    toHexString: (num: number) => string;
    stringify: (obj: any, replacer?: (any: any) => any, space?: (number | string), depth?: number) => string;
    parse: (str: string) => any;
};
