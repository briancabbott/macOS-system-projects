//
//  Calendar+Codable.swift
//  Codable


import Foundation

extension Calendar : Codable {

    internal static func _toNSCalendarIdentifier(_ identifier : Identifier) -> NSCalendar.Identifier {
        if #available(OSX 10.10, iOS 8.0, *) {
            let identifierMap : [Identifier : NSCalendar.Identifier] =
                [.gregorian : .gregorian,
                 .buddhist : .buddhist,
                 .chinese : .chinese,
                 .coptic : .coptic,
                 .ethiopicAmeteMihret : .ethiopicAmeteMihret,
                 .ethiopicAmeteAlem : .ethiopicAmeteAlem,
                 .hebrew : .hebrew,
                 .iso8601 : .ISO8601,
                 .indian : .indian,
                 .islamic : .islamic,
                 .islamicCivil : .islamicCivil,
                 .japanese : .japanese,
                 .persian : .persian,
                 .republicOfChina : .republicOfChina,
                 .islamicTabular : .islamicTabular,
                 .islamicUmmAlQura : .islamicUmmAlQura]
            return identifierMap[identifier]!
        } else {
            let identifierMap : [Identifier : NSCalendar.Identifier] =
                [.gregorian : .gregorian,
                 .buddhist : .buddhist,
                 .chinese : .chinese,
                 .coptic : .coptic,
                 .ethiopicAmeteMihret : .ethiopicAmeteMihret,
                 .ethiopicAmeteAlem : .ethiopicAmeteAlem,
                 .hebrew : .hebrew,
                 .iso8601 : .ISO8601,
                 .indian : .indian,
                 .islamic : .islamic,
                 .islamicCivil : .islamicCivil,
                 .japanese : .japanese,
                 .persian : .persian,
                 .republicOfChina : .republicOfChina]
            return identifierMap[identifier]!
        }
    }

    internal static func _fromNSCalendarIdentifier(_ identifier : NSCalendar.Identifier) -> Identifier {
        if #available(OSX 10.10, iOS 8.0, *) {
            let identifierMap : [NSCalendar.Identifier : Identifier] =
                [.gregorian : .gregorian,
                 .buddhist : .buddhist,
                 .chinese : .chinese,
                 .coptic : .coptic,
                 .ethiopicAmeteMihret : .ethiopicAmeteMihret,
                 .ethiopicAmeteAlem : .ethiopicAmeteAlem,
                 .hebrew : .hebrew,
                 .ISO8601 : .iso8601,
                 .indian : .indian,
                 .islamic : .islamic,
                 .islamicCivil : .islamicCivil,
                 .japanese : .japanese,
                 .persian : .persian,
                 .republicOfChina : .republicOfChina,
                 .islamicTabular : .islamicTabular,
                 .islamicUmmAlQura : .islamicUmmAlQura]
            return identifierMap[identifier]!
        } else {
            let identifierMap : [NSCalendar.Identifier : Identifier] =
                [.gregorian : .gregorian,
                 .buddhist : .buddhist,
                 .chinese : .chinese,
                 .coptic : .coptic,
                 .ethiopicAmeteMihret : .ethiopicAmeteMihret,
                 .ethiopicAmeteAlem : .ethiopicAmeteAlem,
                 .hebrew : .hebrew,
                 .ISO8601 : .iso8601,
                 .indian : .indian,
                 .islamic : .islamic,
                 .islamicCivil : .islamicCivil,
                 .japanese : .japanese,
                 .persian : .persian,
                 .republicOfChina : .republicOfChina]
            return identifierMap[identifier]!
        }
    }

    private enum CodingKeys : Int, CodingKey {
        case identifier
        case locale
        case timeZone
        case firstWeekday
        case minimumDaysInFirstWeek
    }

    public init(from decoder: Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        let identifierString = try container.decode(String.self, forKey: .identifier)
        let identifier = Calendar._fromNSCalendarIdentifier(NSCalendar.Identifier(rawValue: identifierString))
        self.init(identifier: identifier)

        self.locale = try container.decodeIfPresent(Locale.self, forKey: .locale)
        self.timeZone = try container.decode(TimeZone.self, forKey: .timeZone)
        self.firstWeekday = try container.decode(Int.self, forKey: .firstWeekday)
        self.minimumDaysInFirstWeek = try container.decode(Int.self, forKey: .minimumDaysInFirstWeek)
    }

    public func encode(to encoder: Encoder) throws {
        var container = encoder.container(keyedBy: CodingKeys.self)

        let identifier = Calendar._toNSCalendarIdentifier(self.identifier).rawValue
        try container.encode(identifier, forKey: .identifier)
        try container.encode(self.locale, forKey: .locale)
        try container.encode(self.timeZone, forKey: .timeZone)
        try container.encode(self.firstWeekday, forKey: .firstWeekday)
        try container.encode(self.minimumDaysInFirstWeek, forKey: .minimumDaysInFirstWeek)
    }
}
