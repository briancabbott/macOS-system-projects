// RUN: %target-swift-frontend -emit-sil %s -verify

func non_fully_covered_switch(x: Int) -> Int {
  var x = x
  switch x {
    case 0:
     x++
    case 3:
     x--
  } // expected-error{{switch must be exhaustive}}
  return x;
}

