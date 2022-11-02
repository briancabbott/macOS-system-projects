//
//  main.swift
//  bf
//
//  Created by Dan Kogai on 6/26/14.
//  Copyright (c) 2014 Dan Kogai. All rights reserved.
//
// echo with all bf commands.
// minimum echo would be "+[,.]" suffice
//println(bfc("+[,<>.]-"))
import BF
// print(BF("+[,<>.]-")!.run(input:"Hello, Swift!"))
print(BF.compile(src:"+[,<>.]-"))
