class Snobol4 < Formula
  desc "String oriented and symbolic programming language"
  homepage "https://www.snobol4.org/"
  url "ftp://ftp.ultimate.com/snobol/old/snobol4-1.5.tar.gz"
  mirror "https://src.fedoraproject.org/lookaside/pkgs/snobol/snobol4-1.5.tar.gz/54ac3ddd51fb34ec63b1eb0ae7b99794/snobol4-1.5.tar.gz"
  sha256 "9f7ec649f2d700a30091af3bbd68db90b916d728200f915b1ba522bcfd0d7abd"
  license "BSD-2-Clause"

  bottle do
    sha256 "14172b3d9e48c6b7401c5fa84c3169fef2fcb72ed023ce87779950b7cc8210cf" => :big_sur
    sha256 "deab0b471d4e29f0bd4eefd010b479190d2e940b2f962bc12080c32e1cdf1aaf" => :arm64_big_sur
    sha256 "d1902c73b221c4ed0e97c6a9da03d154dbaf81e18b8420ff2e4bbd320c349644" => :catalina
    sha256 "836e69e4b55f8e061d3862b0f52b7c9800a224e4186bb2116f5d2121b4ed4f79" => :mojave
    sha256 "9282b4f4887f0e031321314fcb4ed9af82b7f023c2c20f8cf7b7d278c098424b" => :high_sierra
    sha256 "2c8d1b2a54a3a3f0d810c88bc0a2545dbea08f73b57dda6052c4de27bdde62ee" => :sierra
    sha256 "f4ee5ba3a933998e7ea1493bab469f00f4ddd13a3e8458002ee43ba6f0cd0e74" => :el_capitan
    sha256 "6903e1b05a795eae13f2f97fd2f1f4b883b03e1c94ba28e3747b3df98c6a955d" => :yosemite
    sha256 "b76b9e5bbeccd4b879b2b3c450f3388b82f4641d4d414e0f2d83768eeb0c058b" => :mavericks
  end

  def install
    system "./configure", "--prefix=#{prefix}", "--mandir=#{man}"
    system "make", "install"
  end
end
