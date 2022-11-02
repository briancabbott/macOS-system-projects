class Greed < Formula
  desc "Game of consumption"
  homepage "http://www.catb.org/~esr/greed/"
  url "http://www.catb.org/~esr/greed/greed-4.2.tar.gz"
  sha256 "702bc0314ddedb2ba17d4b55d873384a1606886e8d69f35ce67f6e3024a8d3fd"
  license "BSD-2-Clause"
  head "https://gitlab.com/esr/greed.git"

  bottle do
    cellar :any_skip_relocation
    rebuild 1
    sha256 "15791321c59787d6e5b633efb195e0b7edf8c92976d5c6991e12a920a9f46a00" => :big_sur
    sha256 "197eaf64e266d04b451278067451a05271ee348e04c860c360212f22e0a22cd2" => :arm64_big_sur
    sha256 "64d0028754d683a8bbe1de0bb1a7319dcf6d8020c6d3624e58df5b5be3bf4e42" => :catalina
    sha256 "9cba951e4fd73d29a1e4899a4f2a7d5f0158f6f5b6d02bb75837c7296530e65c" => :mojave
    sha256 "9685dcc52ad08b19964cfb61f4fd0d9e28ec0d42cde2f112da4e9be1e1d15b5b" => :high_sierra
  end

  def install
    # Handle hard-coded destination
    inreplace "Makefile", "/usr/share/man/man6", man6
    # Make doesn't make directories
    bin.mkpath
    man6.mkpath
    (var/"greed").mkpath
    # High scores will be stored in var/greed
    system "make", "SFILE=#{var}/greed/greed.hs"
    system "make", "install", "BIN=#{bin}"
  end

  def caveats
    <<~EOS
      High scores will be stored in the following location:
        #{var}/greed/greed.hs
    EOS
  end

  test do
    assert_predicate bin/"greed", :executable?
  end
end
