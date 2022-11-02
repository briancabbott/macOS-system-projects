class Libxvmc < Formula
  desc "X.Org: X-Video Motion Compensation API"
  homepage "https://www.x.org/"
  url "https://www.x.org/archive/individual/lib/libXvMC-1.0.12.tar.bz2"
  sha256 "6b3da7977b3f7eaf4f0ac6470ab1e562298d82c4e79077765787963ab7966dcd"
  license "MIT"

  bottle do
    cellar :any
    sha256 "e6921d091fcd38b2ba85587864b3a4c2e2834fd74e985d82cfffd971c6d06ffd" => :big_sur
    sha256 "32885178eaa7d79ea1002cd23e9f2a1552a6cd3636b33863e101b3fd94bef57a" => :arm64_big_sur
    sha256 "25bf67d5ac709c8a1551f38dc7da22bc5a2007b1a0d56589e96f172d35248f7e" => :catalina
    sha256 "aa97b7fa2fa9c9a6df4cdd7bd939b2febfe870dd3feed76214d9f41789d50ab8" => :mojave
    sha256 "b00b2e0ba733b3cd0ad8979cce06353e13a9474c5be32e53a467b8308b78f99a" => :high_sierra
  end

  depends_on "pkg-config" => :build
  depends_on "util-macros" => :build
  depends_on "xorgproto" => :build
  depends_on "libx11"
  depends_on "libxext"
  depends_on "libxv"

  def install
    args = %W[
      --prefix=#{prefix}
      --sysconfdir=#{etc}
      --localstatedir=#{var}
      --disable-dependency-tracking
      --disable-silent-rules
    ]

    system "./configure", *args
    system "make"
    system "make", "install"
  end

  test do
    (testpath/"test.c").write <<~EOS
      #include "X11/Xlib.h"
      #include "X11/extensions/XvMClib.h"

      int main(int argc, char* argv[]) {
        XvPortID *port_id;
        return 0;
      }
    EOS
    system ENV.cc, "test.c"
    assert_equal 0, $CHILD_STATUS.exitstatus
  end
end
