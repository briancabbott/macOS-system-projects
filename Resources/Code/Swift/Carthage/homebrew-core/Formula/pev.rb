class Pev < Formula
  desc "PE analysis toolkit"
  homepage "https://pev.sourceforge.io/"
  url "https://downloads.sourceforge.net/project/pev/pev-0.81/pev-0.81.tar.gz"
  sha256 "921b2831ca956aedc272d8580b2ff1a2cb54fb895cabeb81c907fe62b6ac83fb"
  license "GPL-2.0-or-later"
  head "https://github.com/merces/pev.git"

  livecheck do
    url :stable
  end

  bottle do
    sha256 "bd9160da3191fbdc8c251c6513ae6ca73a330575171e7742e2488f655999d864" => :big_sur
    sha256 "9a6e1d64960daa44838f688dc596cb7ca02536521c7c39ee5349021870f41172" => :arm64_big_sur
    sha256 "d1effa68a21b99e2ed18ad0b0ceb5b4732ec253a818d4d88204801b02bba43ed" => :catalina
    sha256 "0ff3ab7fe514f498dd088d42fd60e63bbd5c7fb3d94222aac68c5a4302404f2f" => :mojave
  end

  depends_on "openssl@1.1"
  depends_on "pcre"

  def install
    ENV.deparallelize
    system "make", "prefix=#{prefix}", "CC=#{ENV.cc}"
    system "make", "prefix=#{prefix}", "install"
  end

  test do
    system "#{bin}/pedis", "--version"
  end
end
