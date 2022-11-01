//
//  Date+Extensions.swift
//  BFKit-Swift
//
//  The MIT License (MIT)
//
//  Copyright (c) 2015 - 2019 Fabrizio Brancati.
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in all
//  copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//  SOFTWARE.

import Foundation

// MARK: - Date extension

/// This extension add some useful functions to Date.
public extension Date {
    // MARK: - Variables
    
    /// Set and get current year.
    var year: Int {
        get {
            #if os(Linux)
                let calendar = Calendar(identifier: .gregorian)
            #else
                let calendar = Calendar.autoupdatingCurrent
            #endif
            
            return calendar.component(.year, from: self)
        }
        set {
            update(components: [.year: newValue])
        }
    }
    
    /// Set and get current month.
    var month: Int {
        get {
            #if os(Linux)
                let calendar = Calendar(identifier: .gregorian)
            #else
                let calendar = Calendar.autoupdatingCurrent
            #endif
            
            return calendar.component(.month, from: self)
        }
        set {
            update(components: [.month: newValue])
        }
    }
    
    /// Set and get current day.
    var day: Int {
        get {
            #if os(Linux)
                let calendar = Calendar(identifier: .gregorian)
            #else
                let calendar = Calendar.autoupdatingCurrent
            #endif
            
            return calendar.component(.day, from: self)
        }
        set {
            update(components: [.day: newValue])
        }
    }
    
    /// Set and get current hour.
    var hour: Int {
        get {
            #if os(Linux)
                let calendar = Calendar(identifier: .gregorian)
            #else
                let calendar = Calendar.autoupdatingCurrent
            #endif
            
            return calendar.component(.hour, from: self)
        }
        set {
            update(components: [.hour: newValue])
        }
    }
    
    /// Set and get current minute.
    var minute: Int {
        get {
            #if os(Linux)
                let calendar = Calendar(identifier: .gregorian)
            #else
                let calendar = Calendar.autoupdatingCurrent
            #endif
            
            return calendar.component(.minute, from: self)
        }
        set {
            update(components: [.minute: newValue])
        }
    }
    
    /// Set and get current second.
    var second: Int {
        get {
            #if os(Linux)
                let calendar = Calendar(identifier: .gregorian)
            #else
                let calendar = Calendar.autoupdatingCurrent
            #endif
            
            return calendar.component(.second, from: self)
        }
        set {
            update(components: [.second: newValue])
        }
    }
    
    /// Get current nanosecond.
    var nanosecond: Int {
        #if os(Linux)
            let calendar = Calendar(identifier: .gregorian)
        #else
            let calendar = Calendar.autoupdatingCurrent
        #endif
        
        return calendar.component(.nanosecond, from: self)
    }
    
    /// Get the weekday number from 
    /// - 1 - Sunday.
    /// - 2 - Monday.
    /// - 3 - Tuerday.
    /// - 4 - Wednesday.
    /// - 5 - Thursday.
    /// - 6 - Friday.
    /// - 7 - Saturday.
    var weekday: Int {
        #if os(Linux)
            let calendar = Calendar(identifier: .gregorian)
        #else
            let calendar = Calendar.autoupdatingCurrent
        #endif
        
        return calendar.component(.weekday, from: self)
    }
    
    /// Editable date components.
    ///
    /// - year: Year component.
    /// - month: Month component.
    /// - day: Day component.
    /// - hour: Hour component.
    /// - minute: Minute component.
    /// - second: Second component.
    enum EditableDateComponents: Int {
        case year
        case month
        case day
        case hour
        case minute
        case second
    }
    
    // MARK: - Functions
    
    /// Update current Date components.
    ///
    /// - Parameters:
    ///   - components: Dictionary of components and values to be updated.
    mutating func update(components: [EditableDateComponents: Int]) {
        #if os(Linux)
            let calendar = Calendar(identifier: .gregorian)
        #else
            let calendar = Calendar.autoupdatingCurrent
        #endif
        
        var dateComponents = calendar.dateComponents([.year, .month, .day, .weekday, .hour, .minute, .second, .nanosecond], from: self)
        
        for (component, value) in components {
            switch component {
            case .year:
                dateComponents.year = value

            case .month:
                dateComponents.month = value

            case .day:
                dateComponents.day = value

            case .hour:
                dateComponents.hour = value

            case .minute:
                dateComponents.minute = value

            case .second:
                dateComponents.second = value
            }
        }
        
        guard let date = calendar.date(from: dateComponents) else {
            return
        }
        
        self = date
    }
    
    /// Creates a Date object from year, month and day as Int.
    ///
    /// - Parameters:
    ///   - year: Year.
    ///   - month: Month.
    ///   - day: Day.
    ///   - hour: Hour.
    ///   - minute: Minute.
    ///   - second: Second.
    init?(year: Int, month: Int, day: Int, hour: Int = 0, minute: Int = 0, second: Int = 0) {
        var components = DateComponents()
        components.year = year
        components.month = month
        components.day = day
        components.hour = hour
        components.minute = minute
        components.second = second
        
        #if os(Linux)
            let calendar = Calendar(identifier: .gregorian)
        #else
            let calendar = Calendar.autoupdatingCurrent
        #endif
        
        guard let date = calendar.date(from: components) else {
            return nil
        }
        self = date
    }
    
    /// Creates a Date object from a date String in a given format.
    ///
    /// - Parameters:
    ///   - dateString: Date String.
    ///   - format: Date String format. Default is "yyyy-MM-dd". Example: "2014-05-20".
    ///   - locale: Locale, default is "en_US_POSIX". You can use Locale.current.identifier.
    init?(parse dateString: String, format: String = "yyyy-MM-dd", locale: String = "en_US_POSIX") {
        #if os(Linux)
            let calendar = Calendar(identifier: .gregorian)
        #else
            let calendar = Calendar.autoupdatingCurrent
        #endif
        
        let dateFormatter = DateFormatter()
        dateFormatter.calendar = calendar
        dateFormatter.locale = Locale(identifier: locale)
        dateFormatter.timeZone = TimeZone.current
        dateFormatter.dateFormat = format
        guard let parsed = dateFormatter.date(from: dateString) else {
            return nil
        }
        
        self = parsed
    }
    
    /// Create a Date with other two Date objects.
    /// Taken from the first date: day, month and year.
    /// Taken from the second date: hours and minutes.
    ///
    /// - Parameters:
    ///   - date: The first date for date.
    ///   - time: The second date for time.
    ///   - dateSeparator: Date separator, default is "-".
    ///   - timeSeparator: Time separator, default is ":".
    init?(date: Date, time: Date, dateSeparator: String = "-", timeSeparator: String = ":") {
        let dateFormatter = DateFormatter()
        dateFormatter.dateFormat = "yyyy\(dateSeparator)MM\(dateSeparator)dd"
        let datePortion: String = dateFormatter.string(from: date)
        
        dateFormatter.dateFormat = "HH\(timeSeparator)mm"
        let timePortion: String = dateFormatter.string(from: time)
        
        let dateTime = "\(datePortion) \(timePortion)"
        dateFormatter.dateFormat = "yyyy\(dateSeparator)MM\(dateSeparator)dd HH\(timeSeparator)mm"
        
        guard let parsed = dateFormatter.date(from: dateTime) else {
            return nil
        }
        self = parsed
    }
    
    /// Create an ISO 8601 date from a String.
    ///
    /// - Parameter date: ISO 8601 String.
    init?(iso8601: String) {
        guard let date = Date(parse: iso8601, format: "yyyy-MM-dd'T'HH:mm:ss.SSSZ") else {
            return nil
        }
        
        self = date
    }
    
    #if !os(Linux)
        /// Creates an ISO 8601 String form 
        ///
        /// - Returns: Returns an ISO 8601 String form 
        func iso8601() -> String {
            let dateFormatter = DateFormatter()
            dateFormatter.calendar = Calendar(identifier: .iso8601)
            dateFormatter.locale = Locale(identifier: "en_US_POSIX")
            dateFormatter.timeZone = TimeZone(secondsFromGMT: 0)
            dateFormatter.dateFormat = "yyyy-MM-dd'T'HH:mm:ss.SSSXXXXX"
            
            return dateFormatter.string(from: self)
        }
    #endif

    /// Get the months number between self and another date.
    ///
    /// - Parameter toDate: The another date.
    /// - Returns: Returns the months between the two dates.
    func monthsBetween(_ toDate: Date) -> Int {
        #if os(Linux)
            let calendar = Calendar(identifier: .gregorian)
        #else
            let calendar = Calendar.autoupdatingCurrent
        #endif
        
        let components = calendar.dateComponents([.month], from: self, to: toDate)
        
        guard let month = components.month else {
            return 0
        }
        
        return abs(month)
    }
    
    /// Get the days number between self and another date.
    ///
    /// - Parameter anotherDate: The another date.
    /// - Returns: Returns the days between the two dates.
    func daysBetween(_ anotherDate: Date) -> Int {
        let time: TimeInterval = timeIntervalSince(anotherDate)
        return Int(abs(time / 60 / 60 / 24))
    }
    
    /// Returns if self is today.
    ///
    /// - Returns: Returns if self is today.
    func isToday() -> Bool {
        isSame(Date())
    }
    
    /// Compare self with another date.
    ///
    /// - Parameter anotherDate: The another date to compare as Date.
    /// - Returns: Returns true if is same day, otherwise false.
    func isSame(_ anotherDate: Date) -> Bool {
        #if os(Linux)
            let calendar = Calendar(identifier: .gregorian)
        #else
            let calendar = Calendar.autoupdatingCurrent
        #endif
        
        let componentsSelf = calendar.dateComponents([.year, .month, .day], from: self)
        let componentsAnotherDate = calendar.dateComponents([.year, .month, .day], from: anotherDate)
        
        return componentsSelf.year == componentsAnotherDate.year && componentsSelf.month == componentsAnotherDate.month && componentsSelf.day == componentsAnotherDate.day
    }
    
    /// Add days to 
    ///
    /// - Parameter days: The number of days to add.
    /// - Returns: Returns self by adding the gived days number.
    func addingDays(_ days: Int) -> Date? {
        #if os(Linux)
            let calendar = Calendar(identifier: .gregorian)
        #else
            let calendar = Calendar.autoupdatingCurrent
        #endif
        
        return calendar.date(byAdding: .day, value: days, to: self)
    }
    
    /// Add days to 
    ///
    /// - Parameter days: The number of days to add.
    mutating func addDays(_ days: Int) {
        guard let date = addingDays(days) else {
            return
        }
        
        self = date
    }
    
    /// Get the year string from 
    ///
    /// - Returns: Returns the year string from 
    func yearString() -> String {
        let dateFormatter = DateFormatter()
        dateFormatter.dateFormat = "yyyy"
        
        return dateFormatter.string(from: self)
    }
    
    /// Get the String date from 
    ///
    /// - Parameters:
    ///   - format: Date format, default is "yyyy-MM-dd".
    ///   - locale: Locale, default is "en_US_POSIX".
    /// - Returns: Returns the String data from 
    func dateString(format: String = "yyyy-MM-dd", locale: String = "en_US_POSIX") -> String {
        let dateFormatter = DateFormatter()
        dateFormatter.locale = Locale(identifier: locale)
        dateFormatter.dateFormat = format
        
        return dateFormatter.string(from: self)
    }
    
    /// Returns date with the year, month and day only.
    ///
    /// - Returns: Date after removing all components but not year, month and day.
    func shortDate() -> Date {
        #if os(Linux)
            let calendar = Calendar(identifier: .gregorian)
        #else
            let calendar = Calendar.autoupdatingCurrent
        #endif
        
        let components = calendar.dateComponents([.year, .month, .day], from: self)
        
        guard let date = calendar.date(from: components) else {
            return self
        }
        
        return date
    }
    
    /// Check if the given date is less than 
    ///
    /// - Parameter date: Date to compare.
    /// - Returns: Returns a true if self is greater than another one, otherwise false.
    func isGreaterThan(_ date: Date) -> Bool {
        var isGreater = false
        if compare(date) == ComparisonResult.orderedDescending {
            isGreater = true
        }
        
        return isGreater
    }
    
    /// Check if the given date is greater than 
    ///
    /// - Parameter date: Date to compare.
    /// - Returns: Returns a true if self is less than another one, otherwise false.
    func isLessThan(_ date: Date) -> Bool {
        var isLess = false
        if compare(date) == ComparisonResult.orderedAscending {
            isLess = true
        }
        
        return isLess
    }
    
    /// Just an alias for `isSame(_ anotherDate: Date)`.
    ///
    /// - Parameter date: Date to compare.
    /// - Returns: Returns a true if self is equal to another one, otherwise false.
    func isEqual(_ date: Date) -> Bool {
        isSame(date)
    }
    
    /// Create a Date with the yesterday date.
    ///
    /// - Returns: Returns a Date with the yesterday date.
    func yesterday() -> Date {
        var date = self
        date.day -= 1
        
        return date
    }
    
    /// Get weekday as a localized string from current weekday number.
    ///
    /// - Returns: Return weekday as a localized string.
    func localizedWeekday() -> String {
        let dateFormatter = DateFormatter()
        dateFormatter.dateFormat = "EEEE"
        
        if let locale = NSLocale.preferredLanguages.first {
            dateFormatter.locale = Locale(identifier: locale)
        }
        
        return dateFormatter.string(from: self).capitalized
    }
    
    /// Get month as a localized string from current month.
    ///
    /// - Returns: Returns the given month as a localized string.
    func localizedMonth() -> String {
        let dateFormatter = DateFormatter()
        dateFormatter.dateFormat = "MMMM"
        
        if let locale = NSLocale.preferredLanguages.first {
            dateFormatter.locale = Locale(identifier: locale)
        }
        
        return dateFormatter.string(from: self).capitalized
    }
    
    /// Get the given Date structure as a formatted string.
    ///
    /// - Parameters:
    ///   - info: The Date to be formatted.
    ///   - dateSeparator: The string to be used as date separator. (Currently does not work on Linux).
    ///   - usFormat: Set if the timestamp is in US format or not.
    ///   - nanosecond: Set if the timestamp has to have the nanosecond.
    /// - Returns: Returns a String in the following format (dateSeparator = "/", usFormat to false and nanosecond to false). D/M/Y H:M:S. Example: 15/10/2013 10:38:43.
    func description(dateSeparator: String = "/", usFormat: Bool = false, nanosecond: Bool = false) -> String {
        var description: String
        
        #if os(Linux)
            if usFormat {
                description = String(format: "%04li-%02li-%02li %02li:%02li:%02li", year, month, day, hour, minute, second)
            } else {
                description = String(format: "%02li-%02li-%04li %02li:%02li:%02li", month, day, year, hour, minute, second)
            }
        #else
            if usFormat {
                description = String(format: "%04li%@%02li%@%02li %02li:%02li:%02li", year, dateSeparator, month, dateSeparator, day, hour, minute, second)
            } else {
                description = String(format: "%02li%@%02li%@%04li %02li:%02li:%02li", month, dateSeparator, day, dateSeparator, year, hour, minute, second)
            }
        #endif
        
        if nanosecond {
            description += String(format: ":%03li", self.nanosecond / 1_000_000)
        }
        
        return description
    }
}
