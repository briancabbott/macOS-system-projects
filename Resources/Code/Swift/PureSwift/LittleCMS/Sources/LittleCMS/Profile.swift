//
//  Profile.swift
//  LittleCMS
//
//  Created by Alsey Coleman Miller on 6/3/17.
//
//

import struct Foundation.Data
import struct Foundation.Date
import CLCMS

/// A profile that specifies how to interpret a color value for display.
public struct Profile {
    
    // MARK: - Properties
    
    internal var internalReference: CopyOnWrite<Reference>
    
    // MARK: - Initialization
    
    internal init(_ internalReference: Reference) {
        
        self.internalReference = CopyOnWrite(internalReference)
    }
    
    public init?(file: String, context: Context? = nil) {
        
        guard let internalReference = Reference(file: file, context: context)
            else { return nil }
        
        self.internalReference = CopyOnWrite(internalReference)
    }
    
    public init?(data: Data, context: Context? = nil) {
        
        guard let internalReference = Reference(data: data, context: context)
            else { return nil }
        
        self.internalReference = CopyOnWrite(internalReference)
    }
    
    /// Creates a gray profile based on White point and transfer function.
    /// It populates following tags:
    /// - `cmsSigProfileDescriptionTag`
    /// - `cmsSigMediaWhitePointTag`
    /// - `cmsSigGrayTRCTag`
    ///
    /// - Parameter whitePoint: White point of the gray device or space.
    /// - Parameter toneCurve: Tone curve describing the device or space gamma.
    /// - Parameter context: Optional `Context` object that keeps track of all plug-ins, static data and logging.
    /// - Returns: An ICC profile object on success, `nil` on error.
    public init?(grey whitePoint: cmsCIExyY, toneCurve: ToneCurve, context: Context? = nil) {
        
        guard let internalReference = Reference(grey: whitePoint, toneCurve: toneCurve, context: context)
            else { return nil }
        
        self.internalReference = CopyOnWrite(internalReference)
    }
    
    public init?(sRGB context: Context?) {
        
        guard let internalReference = Reference(sRGB: context)
            else { return nil }
        
        self.internalReference = CopyOnWrite(internalReference)
    }
    
    /// Creates a Lab->Lab identity, marking it as v2 ICC profile.
    ///
    /// - Note: Adjustments for accomodating PCS endoing shall be done by Little CMS when using this profile.
    public init?(lab2 whitePoint: cmsCIExyY, context: Context? = nil) {
        
        guard let internalReference = Reference(lab2: whitePoint, context: context)
            else { return nil }
        
        self.internalReference = CopyOnWrite(internalReference)
    }
    
    /// Creates a Lab->Lab identity, marking it as v4 ICC profile.
    public init?(lab4 whitePoint: cmsCIExyY, context: Context? = nil) {
        
        guard let internalReference = Reference(lab4: whitePoint, context: context)
            else { return nil }
        
        self.internalReference = CopyOnWrite(internalReference)
    }
    
    /// This is a devicelink operating in CMYK for ink-limiting.
    public init?(inkLimitingDeviceLink colorspace: ColorSpaceSignature, limit: Double, context: Context? = nil) {
        
        guard let internalReference = Reference(inkLimitingDeviceLink: colorspace, limit: limit, context: context)
            else { return nil }
        
        self.internalReference = CopyOnWrite(internalReference)
    }
    
    // MARK: - Accessors
    
    public var signature: ColorSpaceSignature {
        
        return internalReference.reference.signature
    }
    
    /// Profile connection space used by the given profile, using the ICC convention.
    public var connectionSpace: ColorSpaceSignature {
        
        get { return internalReference.reference.connectionSpace }
        
        mutating set { internalReference.mutatingReference.connectionSpace = newValue }
    }
    
    /// Returns the date and time when profile was created. 
    /// This is a field stored in profile header.
    var creation: Date? {
        
        return internalReference.reference.creation
    }
    
    // MARK: - Methods
    
    /// Saves the contents of a profile to `Data`.
    public func save() -> Data? {
        
        return internalReference.reference.save()
    }
    
    // MARK: - Subscript
    
    /// Get the string for the specified profile info with the default locale.
    public subscript (infoType: Info) -> String? {
        
        return internalReference.reference[infoType]
    }
    
    /// Get the string for the specified profile info and locale.
    public subscript (infoType: Info, locale: (languageCode: String, countryCode: String)) -> String? {
        
        return internalReference.reference[infoType, locale]
    }
}

// MARK: - Reference Type

internal extension Profile {
    
    internal final class Reference {
        
        // MARK: - Properties
        
        internal let internalPointer: cmsHPROFILE
        
        public let context: Context?
        
        // MARK: - Initialization
        
        deinit {
            
            // deallocate profile
            cmsCloseProfile(internalPointer)
        }
        
        /// Creates a fake NULL profile. 
        ///
        /// This profile return 1 channel as always 0. 
        ///
        /// Is useful only for gamut checking tricks.
        @inline(__always)
        internal init?(null context: Context? = nil) {
            
            guard let internalPointer = cmsCreateNULLProfileTHR(context?.internalPointer)
                else { return nil }
            
            self.internalPointer = internalPointer
            self.context = context
        }
        
        @inline(__always)
        private init?(file: String, access: FileAccess, context: Context? = nil) {
            
            guard let internalPointer = cmsOpenProfileFromFileTHR(context?.internalPointer, file, access.rawValue)
                else { return nil }
            
            self.internalPointer = internalPointer
            self.context = context
        }
        
        convenience init?(file: String, context: Context? = nil) {
            
            // only allow reading files
            self.init(file: file, access: .read, context: context)
        }
        
        @inline(__always)
        init?(data: Data, context: Context? = nil) {
            
            guard let internalPointer = data.withUnsafeBytes({ cmsOpenProfileFromMemTHR(context?.internalPointer, $0, cmsUInt32Number(data.count)) })
                else { return nil }
            
            self.internalPointer = internalPointer
            self.context = context
        }
        
        @inline(__always)
        init?(grey whitePoint: cmsCIExyY, toneCurve: ToneCurve, context: Context? = nil) {
            
            var whiteCIExyY = whitePoint
            
            let table = toneCurve.internalPointer
            
            guard let internalPointer = cmsCreateGrayProfileTHR(context?.internalPointer, &whiteCIExyY, table)
                else { return nil }
            
            self.internalPointer = internalPointer
            self.context = context
        }
        
        @inline(__always)
        init?(sRGB context: Context?) {
            
            guard let internalPointer = cmsCreate_sRGBProfileTHR(context?.internalPointer)
                else { return nil }
            
            self.internalPointer = internalPointer
            self.context = context
        }
        
        /// Creates a Lab->Lab identity, marking it as v2 ICC profile.
        ///
        /// - Note: Adjustments for accomodating PCS endoing shall be done by Little CMS when using this profile.
        @inline(__always)
        init?(lab2 whitePoint: cmsCIExyY, context: Context? = nil) {
            
            var whitePoint = whitePoint
            
            guard let internalPointer = cmsCreateLab2ProfileTHR(context?.internalPointer, &whitePoint)
                else { return nil }
            
            self.internalPointer = internalPointer
            self.context = context
        }
        
        /// Creates a Lab->Lab identity, marking it as v4 ICC profile.
        @inline(__always)
        init?(lab4 whitePoint: cmsCIExyY, context: Context? = nil) {
            
            var whitePoint = whitePoint
            
            guard let internalPointer = cmsCreateLab4ProfileTHR(context?.internalPointer, &whitePoint)
                else { return nil }
            
            self.internalPointer = internalPointer
            self.context = context
        }
        
        /// This is a devicelink operating in CMYK for ink-limiting.
        @inline(__always)
        init?(inkLimitingDeviceLink colorspace: ColorSpaceSignature, limit: Double, context: Context? = nil) {
            
            guard let internalPointer = cmsCreateInkLimitingDeviceLinkTHR(context?.internalPointer, colorspace, limit)
                else { return nil }
            
            self.internalPointer = internalPointer
            self.context = context
        }
        
        // MARK: - Accessors
        
        var copy: Profile.Reference? {
            
            // get copy by creating data, expensive, but no other way
            guard let data = self.save(),
                let new = Profile.Reference(data: data, context: self.context)
                else { return nil }
            
            return new
        }
        
        var creation: Date? {
            
            var time = tm()
            
            guard cmsGetHeaderCreationDateTime(internalPointer, &time) > 0
                else { return nil }
            
            let dateComponents = DateComponents(brokenDown: time)
            
            return dateComponents.date
        }
        
        var signature: ColorSpaceSignature {
            
            @inline(__always)
            get { return cmsGetColorSpace(internalPointer) }
        }
        
        /// Profile connection space used by the given profile, using the ICC convention.
        var connectionSpace: ColorSpaceSignature {
            
            @inline(__always)
            get { return cmsGetPCS(internalPointer) }
            
            @inline(__always)
            set { cmsSetPCS(internalPointer, newValue) }
        }
        
        /// Returns the number of tags present in a given profile.
        var tagCount: Int {
            
            @inline(__always)
            get { return Int(cmsGetTagCount(internalPointer)) }
        }
        
        // MARK: - Methods
        
        /// Saves the contents of a profile to `Data`.
        func save() -> Data? {
            
            var length: cmsUInt32Number = 0
            
            guard cmsSaveProfileToMem(internalPointer, nil, &length) > 0
                else { return nil }
            
            var data = Data(count: Int(length))
            
            guard data.withUnsafeMutableBytes({ cmsSaveProfileToMem(self.internalPointer, $0, nil) }) != 0
                else { return nil }
            
            return data
        }
        
        // Returns `true` if a tag with signature sig is found on the profile.
        /// Useful to check if a profile contains a given tag.
        @inline(__always)
        func contains(_ tag: Tag) -> Bool {
            
            return cmsIsTag(internalPointer, tag) > 0
        }
        
        /// Creates a directory entry on tag sig that points to same location as tag destination.
        /// Using this function you can collapse several tag entries to the same block in the profile.
        @inline(__always)
        func link(_ tag: cmsTagSignature, to destination: cmsTagSignature) -> Bool {
            
            return cmsLinkTag(internalPointer, tag, destination) > 0
        }
        
        /// Returns the tag linked to, in the case two tags are sharing same resource,
        /// or `nil` if the tag is not linked to any other tag.
        @inline(__always)
        func tagLinked(to tag: cmsTagSignature) -> cmsTagSignature? {
            
            let tag = cmsTagLinkedTo(internalPointer, tag)
            
            guard tag.isValid else { return nil }
            
            return tag
        }
        
        /// Returns the signature of a tag located at the specified index. 
        @inline(__always)
        public func tag(at index: Int) -> Tag? {
            
            let tag = cmsGetTagSignature(internalPointer, cmsUInt32Number(index))
            
            guard tag.isValid else { return nil }
            
            return tag
        }
        
        // MARK: - Subscript
        
        subscript (infoType: Info) -> String? {
            
            @inline(__always)
            get { return self[infoType, (cmsNoLanguage, cmsNoCountry)] }
        }
        
        /// Get the string for the specified profile info.
        subscript (infoType: Info, locale: (languageCode: String, countryCode: String)) -> String? {
            
            let info = cmsInfoType(infoType)
            
            // get buffer size
            let bufferSize = cmsGetProfileInfo(internalPointer, info, locale.languageCode, locale.countryCode, nil, 0)
            
            guard bufferSize > 0 else { return nil }
            
            // allocate buffer and get data
            var data = Data(repeating: 0, count: Int(bufferSize))
            
            guard data.withUnsafeMutableBytes({ cmsGetProfileInfo(internalPointer, info, locale.languageCode, locale.countryCode, UnsafeMutablePointer<wchar_t>($0), bufferSize) }) != 0 else { fatalError("Cannot get data for \(infoType)") }
            
            assert(wchar_t.self == Int32.self, "wchar_t is \(wchar_t.self)")
            
            return String(littleCMS: data)
        }
    }
}

// MARK: - Equatable

extension Profile: Equatable {
    
    public static func == (lhs: Profile, rhs: Profile) -> Bool {
        
        guard let lhsData = lhs.save(),
            let rhsData = rhs.save()
            else { return false }
        
        return lhsData == rhsData
    }
}

// MARK: - Supporting Types

public extension Profile {
    
    public enum Info {
        
        case description
        case manufacturer
        case model
        case copyright
    }
    
    fileprivate enum FileAccess: String {
        
        case read = "r"
        case write = "w"
    }
}

// MARK: - Little CMS Extensions / Helpers

fileprivate extension String {
    
    init?(littleCMS data: Data) {
        
        // try to decode data into string
        let possibleEncodings: [String.Encoding] = [.utf32, .utf32LittleEndian, .utf32BigEndian]
        
        var value: String?
        
        for encoding in possibleEncodings {
            
            guard let string = String(data: data, encoding: encoding)
                else { continue }
            
            value = string
        }
        
        if let value = value {
            
            self = value
            
        } else {
            
            return nil
        }
    }
}

public extension cmsInfoType {
    
    init(_ info: Profile.Info) {
        
        switch info {
        case .description:  self = cmsInfoDescription
        case .manufacturer: self = cmsInfoManufacturer
        case .model:        self = cmsInfoModel
        case .copyright:    self = cmsInfoCopyright
        }
    }
}

// MARK: - Internal Protocols

extension Profile.Reference: CopyableHandle { }

extension Profile.Reference: ContextualHandle {
    static var cmsGetContextID: cmsGetContextIDFunction { return cmsGetProfileContextID }
}
