class Dcraw < Formula
  desc "Digital camera RAW photo decoding software"
  homepage "https://www.dechifro.org/dcraw/"
  url "https://www.dechifro.org/dcraw/archive/dcraw-9.28.0.tar.gz"
  mirror "https://mirrorservice.org/sites/distfiles.macports.org/dcraw/dcraw-9.28.0.tar.gz"
  sha256 "2890c3da2642cd44c5f3bfed2c9b2c1db83da5cec09cc17e0fa72e17541fb4b9"

  livecheck do
    url "https://distfiles.macports.org/dcraw/"
    regex(/href=.*?dcraw[._-]v?(\d+(?:\.\d+)+)\.t/i)
  end

  bottle do
    cellar :any
    sha256 "fc0e1b6d2ac47be836929a68ee33d693bb2e455c4ecdd1dee7beafb3a5c123c6" => :big_sur
    sha256 "743f571022d74442fa2d81309c916ff665114303a4e916d1b9a970c50ddb71e3" => :arm64_big_sur
    sha256 "df26056a9b3374154b499b4dbdee4a1417a58a15cffe22ac40f095747ee1f8a7" => :catalina
    sha256 "4673710b946c4fa3eb47d0b693b380e8abb636202ce86e0e13372a8539141bd8" => :mojave
    sha256 "21f31347e500f314a1f2e6fe03f0d6009b25fa5bd9f1f339b0fe77fc38050e81" => :high_sierra
    sha256 "dc99d6de1166a3f4fa66d23b798dad9a58e0fac24f72c02ab38ea32e74b30a9e" => :sierra
    sha256 "022f85e8da7b4cd8c68d7251d39bf3084ec28a15cb859d9cfe49bd439e312466" => :el_capitan
  end

  depends_on "jasper"
  depends_on "jpeg"
  depends_on "little-cms2"

  def install
    ENV.append_to_cflags "-I#{HOMEBREW_PREFIX}/include -L#{HOMEBREW_PREFIX}/lib"
    system ENV.cc, "-o", "dcraw", ENV.cflags, "dcraw.c", "-lm", "-ljpeg", "-llcms2", "-ljasper"
    bin.install "dcraw"
    man1.install "dcraw.1"
  end

  test do
    assert_match "\"dcraw\" v9", shell_output("#{bin}/dcraw", 1)
  end
end
