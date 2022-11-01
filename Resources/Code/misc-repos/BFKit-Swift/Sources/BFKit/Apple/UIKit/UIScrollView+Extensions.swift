//
//  UIScrollView+Extensions.swift
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
import UIKit

// MARK: - UIScrollView extension

/// This extesion adds some useful functions to UIScrollView.
public extension UIScrollView {
    // MARK: - Functions
    
    /// Create an UIScrollView and set some parameters.
    ///
    /// - Parameters:
    ///   - frame: ScrollView frame.
    ///   - contentSize: ScrollView content size.
    ///   - clipsToBounds: Set if ScrollView has to clips to bounds.
    ///   - pagingEnabled: Set if ScrollView has paging enabled.
    ///   - showScrollIndicators: Set if ScrollView has to show the scroll indicators, vertical and horizontal.
    ///   - delegate: ScrollView delegate.
    convenience init(frame: CGRect, contentSize: CGSize, clipsToBounds: Bool, pagingEnabled: Bool, showScrollIndicators: Bool, delegate: UIScrollViewDelegate?) {
        self.init(frame: frame)
        self.delegate = delegate
        isPagingEnabled = pagingEnabled
        self.clipsToBounds = clipsToBounds
        showsVerticalScrollIndicator = showScrollIndicators
        showsHorizontalScrollIndicator = showScrollIndicators
        self.contentSize = contentSize
    }
}
