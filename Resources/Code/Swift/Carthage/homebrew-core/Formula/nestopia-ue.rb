class NestopiaUe < Formula
  desc "NES emulator"
  homepage "http://0ldsk00l.ca/nestopia/"
  url "https://github.com/0ldsk00l/nestopia/archive/1.50.tar.gz"
  sha256 "f0274f8b033852007c67237897c69725b811c0df8a6d0120f39c23e990662aae"
  license "GPL-2.0"
  head "https://github.com/0ldsk00l/nestopia.git"

  bottle do
    sha256 "d6a31de30e416aaa7cb365b22c35555947fa4967d3f0d7b334694e63e70c08a8" => :big_sur
    sha256 "21dd1ec76826e208b26d981ebfafbef95d8c2f4a00330cd7556bc366e5bc7745" => :arm64_big_sur
    sha256 "19acd9260a874dec614062d0362a5936a0d9322e9fe66f0f8426d0dec67a6dd6" => :catalina
    sha256 "e41a57949e9ebeffd1fa72de619da0dc2bbc813adf1b83922a0151362a9b9f04" => :mojave
    sha256 "dc7632deb424cbfd112350aa1ddad0d1b0715cce9ebfda0bbbd8e77640cea044" => :high_sierra
  end

  depends_on "autoconf" => :build
  depends_on "autoconf-archive" => :build
  depends_on "automake" => :build
  depends_on "pkg-config" => :build
  depends_on "libao"
  depends_on "libarchive"
  depends_on "libepoxy"
  depends_on "sdl2"

  def install
    system "autoreconf", "-fiv"
    system "./configure", "--disable-debug",
                          "--disable-dependency-tracking",
                          "--disable-silent-rules",
                          "--prefix=#{prefix}",
                          "--datarootdir=#{pkgshare}"
    system "make", "install"
  end

  test do
    assert_match /Nestopia UE #{version}$/, shell_output("#{bin}/nestopia --version")
  end
end
