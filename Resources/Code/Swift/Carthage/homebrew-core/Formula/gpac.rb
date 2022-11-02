# Installs a relatively minimalist version of the GPAC tools. The
# most commonly used tool in this package is the MP4Box metadata
# interleaver, which has relatively few dependencies.
#
# The challenge with building everything is that Gpac depends on
# a much older version of FFMpeg and WxWidgets than the version
# that Brew installs

class Gpac < Formula
  desc "Multimedia framework for research and academic purposes"
  homepage "https://gpac.wp.mines-telecom.fr/"
  url "https://github.com/gpac/gpac/archive/v1.0.1.tar.gz"
  sha256 "3b0ffba73c68ea8847027c23f45cd81d705110ec47cf3c36f60e669de867e0af"
  license "LGPL-2.1-or-later"
  head "https://github.com/gpac/gpac.git"

  bottle do
    cellar :any
    sha256 "f6c4c5413b6746988520e5d9b1f0ee584f7456b208ed994e87fa8675436c9c41" => :big_sur
    sha256 "9d969e1cab82b163e4958a99e7e73f89fcf7a10675626223c5d4be1fc3b7d427" => :arm64_big_sur
    sha256 "cd323eba25dac7431970a3854c1317c1e4ce71e12421a1c789bfe127f2c373d7" => :catalina
    sha256 "f6acea4aee0a0719ae5c8deb775529a07a7da5d8e32e9c30371a7165b010294d" => :mojave
    sha256 "b050e13507f1462dcf37d968ed24e36195cf6026dc762e7ddbfa7de9088e0a9c" => :high_sierra
  end

  depends_on "pkg-config" => :build
  depends_on "openssl@1.1"

  conflicts_with "bento4", because: "both install `mp42ts` binaries"

  def install
    args = %W[
      --disable-wx
      --disable-pulseaudio
      --prefix=#{prefix}
      --mandir=#{man}
      --disable-x11
    ]

    system "./configure", *args
    system "make"
    system "make", "install"
  end

  test do
    system "#{bin}/MP4Box", "-add", test_fixtures("test.mp3"), "#{testpath}/out.mp4"
    assert_predicate testpath/"out.mp4", :exist?
  end
end
