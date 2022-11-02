class Dnstop < Formula
  desc "Console tool to analyze DNS traffic"
  homepage "http://dns.measurement-factory.com/tools/dnstop/index.html"
  url "http://dns.measurement-factory.com/tools/dnstop/src/dnstop-20140915.tar.gz"
  sha256 "b4b03d02005b16e98d923fa79957ea947e3aa6638bb267403102d12290d0c57a"
  license "BSD-3-Clause"

  bottle do
    cellar :any_skip_relocation
    rebuild 1
    sha256 "c07eca212e72ce354b9e29575efa61f607a9ba43dc07072247f925d331ce7763" => :big_sur
    sha256 "9fba6f2f539b25ef2e918c9600a3027a72188984cad8748f2edd55c59712c414" => :arm64_big_sur
    sha256 "61522feaa64c92d28044e88366555a6f816366671728d71e286960b83a176417" => :catalina
    sha256 "fc741283d3b21ab68de0972c733b38ac01c363a0588254c41ad19f5591f32bda" => :mojave
    sha256 "4d6b9a2f15e3165ccf63b67752cd4f0d21b128f64b5f22beb2c2b0657e082709" => :high_sierra
    sha256 "dc995c2857fdd5093ae753844ce5c45ed00bae59184528a184e0313b25882802" => :sierra
    sha256 "1d5b1ad056475ce9a27f40b48cbbf58421e4eb66fd134ac318413de2d025db66" => :el_capitan
    sha256 "aa3b72d1432e7c13b9b7e0722cde3f7fafef17aff557489662029698929638dc" => :yosemite
    sha256 "4a57a6144a94b3eb2cce64ec7f8f97447eafd1c0be2c5789593920d045e9a189" => :mavericks
  end

  def install
    system "./configure", "--prefix=#{prefix}"
    system "make"
    bin.install "dnstop"
    man8.install "dnstop.8"
  end

  test do
    system "#{bin}/dnstop", "-v"
  end
end
