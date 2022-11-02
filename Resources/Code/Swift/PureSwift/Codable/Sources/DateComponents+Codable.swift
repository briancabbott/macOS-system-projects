//
//  DateComponents+Codable.swift
//  Codable


import Foundation

extension DateComponents : Codable {
    private enum CodingKeys : Int, CodingKey {
        case calendar
        case timeZone
        case era
        case year
        case month
        case day
        case hour
        case minute
        case second
        case nanosecond
        case weekday
        case weekdayOrdinal
        case quarter
        case weekOfMonth
        case weekOfYear
        case yearForWeekOfYear
    }

    public init(from decoder: Decoder) throws {
        let container  = try decoder.container(keyedBy: CodingKeys.self)
        let calendar   = try container.decodeIfPresent(Calendar.self, forKey: .calendar)
        let timeZone   = try container.decodeIfPresent(TimeZone.self, forKey: .timeZone)
        let era        = try container.decodeIfPresent(Int.self, forKey: .era)
        let year       = try container.decodeIfPresent(Int.self, forKey: .year)
        let month      = try container.decodeIfPresent(Int.self, forKey: .month)
        let day        = try container.decodeIfPresent(Int.self, forKey: .day)
        let hour       = try container.decodeIfPresent(Int.self, forKey: .hour)
        let minute     = try container.decodeIfPresent(Int.self, forKey: .minute)
        let second     = try container.decodeIfPresent(Int.self, forKey: .second)
        let nanosecond = try container.decodeIfPresent(Int.self, forKey: .nanosecond)

        let weekday           = try container.decodeIfPresent(Int.self, forKey: .weekday)
        let weekdayOrdinal    = try container.decodeIfPresent(Int.self, forKey: .weekdayOrdinal)
        let quarter           = try container.decodeIfPresent(Int.self, forKey: .quarter)
        let weekOfMonth       = try container.decodeIfPresent(Int.self, forKey: .weekOfMonth)
        let weekOfYear        = try container.decodeIfPresent(Int.self, forKey: .weekOfYear)
        let yearForWeekOfYear = try container.decodeIfPresent(Int.self, forKey: .yearForWeekOfYear)

        self.init(calendar: calendar,
                  timeZone: timeZone,
                  era: era,
                  year: year,
                  month: month,
                  day: day,
                  hour: hour,
                  minute: minute,
                  second: second,
                  nanosecond: nanosecond,
                  weekday: weekday,
                  weekdayOrdinal: weekdayOrdinal,
                  quarter: quarter,
                  weekOfMonth: weekOfMonth,
                  weekOfYear: weekOfYear,
                  yearForWeekOfYear: yearForWeekOfYear)
    }

    public func encode(to encoder: Encoder) throws {
        // TODO: Replace all with encodeIfPresent, when added.
        var container = encoder.container(keyedBy: CodingKeys.self)
        if self.calendar   != nil { try container.encode(self.calendar!, forKey: .calendar) }
        if self.timeZone   != nil { try container.encode(self.timeZone!, forKey: .timeZone) }
        if self.era        != nil { try container.encode(self.era!, forKey: .era) }
        if self.year       != nil { try container.encode(self.year!, forKey: .year) }
        if self.month      != nil { try container.encode(self.month!, forKey: .month) }
        if self.day        != nil { try container.encode(self.day!, forKey: .day) }
        if self.hour       != nil { try container.encode(self.hour!, forKey: .hour) }
        if self.minute     != nil { try container.encode(self.minute!, forKey: .minute) }
        if self.second     != nil { try container.encode(self.second!, forKey: .second) }
        if self.nanosecond != nil { try container.encode(self.nanosecond!, forKey: .nanosecond) }

        if self.weekday           != nil { try container.encode(self.weekday!, forKey: .weekday) }
        if self.weekdayOrdinal    != nil { try container.encode(self.weekdayOrdinal!, forKey: .weekdayOrdinal) }
        if self.quarter           != nil { try container.encode(self.quarter!, forKey: .quarter) }
        if self.weekOfMonth       != nil { try container.encode(self.weekOfMonth!, forKey: .weekOfMonth) }
        if self.weekOfYear        != nil { try container.encode(self.weekOfYear!, forKey: .weekOfYear) }
        if self.yearForWeekOfYear != nil { try container.encode(self.yearForWeekOfYear!, forKey: .yearForWeekOfYear) }
    }
}
