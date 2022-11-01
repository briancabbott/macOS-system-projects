//
//  Post.swift
//  Note
//
//  Created by Devran on 09.02.22.
//

import Foundation

struct Post: Codable {
    var isPositiveVotesLocked: Bool = false
    var positiveVotes: Int = 0
}
