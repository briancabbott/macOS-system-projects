class Convertlit < Formula
  desc "Convert Microsoft Reader format eBooks into open format"
  homepage "http://www.convertlit.com/"
  url "http://www.convertlit.com/clit18src.zip"
  version "1.8"
  sha256 "d70a85f5b945104340d56f48ec17bcf544e3bb3c35b1b3d58d230be699e557ba"

  bottle do
    cellar :any_skip_relocation
    sha256 "4a70dcf4f3bc3b2806794651f1cbbf9effe317ea3d29b06339595bae0d6e71b9" => :big_sur
    sha256 "0ef0e8a30545af331a8acbc7280dfaa41fab75a0bb87a2bf05b84e5ebdc8db2e" => :arm64_big_sur
    sha256 "7d06d34736082be89b9e6c0db2fa42c4d2b4fb15469bef2922003d3d299680c8" => :mojave
    sha256 "f41e31b1f6f53d1441bf670e75e0315f6a0f0e938de75e9973678ed565b6b4b8" => :high_sierra
    sha256 "43e28e7711f27843223b29d351ba0ce03a4deee76bbc99c4bdac50969b8eaeb7" => :sierra
    sha256 "66b05c2c6371f16620c82b31b507413556b511b859644322c65f4ceea4a83a64" => :el_capitan
    sha256 "024a9fdb4b58a3e04c12ec300facbac636b3510f8726726c4be93c60cf272ab1" => :yosemite
    sha256 "366ce6afb71223d3f14939c5d4d382a90cf56df7920cb41dca0eeae72e809702" => :mavericks
  end

  depends_on "libtommath"

  def install
    inreplace "clit18/Makefile" do |s|
      s.gsub! "-I ../libtommath-0.30", "#{HOMEBREW_PREFIX}/include"
      s.gsub! "../libtommath-0.30/libtommath.a", "#{HOMEBREW_PREFIX}/lib/libtommath.a"
    end

    system "make", "-C", "lib"
    system "make", "-C", "clit18"
    bin.install "clit18/clit"
  end
end
