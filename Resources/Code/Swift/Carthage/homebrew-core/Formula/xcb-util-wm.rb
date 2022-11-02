class XcbUtilWm < Formula
  desc "Client and window-manager helpers for EWMH and ICCCM"
  homepage "https://xcb.freedesktop.org"
  url "https://xcb.freedesktop.org/dist/xcb-util-wm-0.4.1.tar.bz2"
  sha256 "28bf8179640eaa89276d2b0f1ce4285103d136be6c98262b6151aaee1d3c2a3f"
  license "X11"

  bottle do
    cellar :any
    sha256 "d63a72b6a9714c0e1d92e1d0da59fc702f1f8aa44dba75c2dcf85fb5a291908d" => :big_sur
    sha256 "88f837cbf6f6693bc54371d9fab65f211951023bd59dddfa2f2084e831abc4a1" => :arm64_big_sur
    sha256 "77611bd19da065ae3e1053ec9b581a52e93bc8669a8efee2d06719632695815f" => :catalina
    sha256 "1423847ca100bb773cd5f85d1766abbf9004b88e85fc92cc25a30ea23341f0e3" => :mojave
  end

  head do
    url "https://gitlab.freedesktop.org/xorg/lib/libxcb-wm.git"

    depends_on "autoconf" => :build
    depends_on "automake" => :build
    depends_on "libtool" => :build
  end

  depends_on "pkg-config" => [:build, :test]
  depends_on "libxcb"

  uses_from_macos "m4" => :build

  def install
    system "./autogen.sh" if build.head?
    system "./configure", "--prefix=#{prefix}",
                          "--sysconfdir=#{etc}",
                          "--localstatedir=#{var}",
                          "--disable-dependency-tracking",
                          "--disable-silent-rules"
    system "make"
    system "make", "install"
  end

  test do
    assert_match "-I#{include}", shell_output("pkg-config --cflags xcb-ewmh")
    assert_match "-I#{include}", shell_output("pkg-config --cflags xcb-icccm")
  end
end
