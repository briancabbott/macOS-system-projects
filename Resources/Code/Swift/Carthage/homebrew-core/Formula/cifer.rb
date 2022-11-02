class Cifer < Formula
  desc "Work on automating classical cipher cracking in C"
  homepage "https://code.google.com/p/cifer/"
  url "https://storage.googleapis.com/google-code-archive-downloads/v2/code.google.com/cifer/cifer-1.2.0.tar.gz"
  sha256 "436816c1f9112b8b80cf974596095648d60ffd47eca8eb91fdeb19d3538ea793"
  license "GPL-3.0"

  bottle do
    cellar :any_skip_relocation
    sha256 "f58f511d07f6a8daf8c868915c1de59f23d33c089da52271da83180e321bab5b" => :big_sur
    sha256 "5f826a8dc0534a56e2c372abeba0f05124f79e96502243d1c89548777d2156b2" => :arm64_big_sur
    sha256 "ce4a7d9b846388eae2309dbd0a1f0493b533cbefef85ae50ff97648b6a46600c" => :catalina
    sha256 "ed647fac83a0f0605c4fbf0492be1568199a60473e20ac455feb4ff1abea1946" => :mojave
    sha256 "04d95a6448d38450079196139c6e6d5b5811265444c9abf8fe93b7424181a222" => :high_sierra
    sha256 "875e676d7866fd3ba2c8b70806838068775ffbc1102c56ca52d041155b2ade43" => :sierra
    sha256 "86cbc00f11a5818f48ee67bdc0fa5f2692cc7f37ae6c2c5eb237338c7dc6919b" => :el_capitan
    sha256 "bde7d97d9ef2a07c481ff8c5ec717fb2ec455fdef864db2a1a7b3056aa1934d2" => :yosemite
    sha256 "f4e2f4024a8daf1e2a1fc071113947561c803503fa242b7df7970e8979cf10be" => :mavericks
  end

  def install
    system "make", "prefix=#{prefix}",
                   "CC=#{ENV.cc}",
                   "CFLAGS=#{ENV.cflags}",
                   "LDFLAGS=#{ENV.ldflags}",
                   "install"
  end

  test do
    assert_match version.to_s, pipe_output("#{bin}/cifer")
  end
end
