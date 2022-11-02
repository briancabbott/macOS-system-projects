class Iprint < Formula
  desc "Provides a print_one function"
  homepage "https://www.samba.org/ftp/unpacked/junkcode/i.c"
  url "https://deb.debian.org/debian/pool/main/i/iprint/iprint_1.3.orig.tar.gz"
  version "1.3-9"
  sha256 "1079b2b68f4199bc286ed08abba3ee326ce3b4d346bdf77a7b9a5d5759c243a3"
  license "GPL-2.0"

  bottle do
    cellar :any_skip_relocation
    rebuild 2
    sha256 "e4508c0b9eed2e203735de1d864dcd4ba35cb7279fb95eef28fbae2cd8d9d41c" => :big_sur
    sha256 "eb538fa6b5466dac71f52ec8f428e4ef0674e1f475893879a857cf27ce914a9f" => :arm64_big_sur
    sha256 "e5fba1fa985ad96aac02d36f50e0985c14248655fd810c15c053e1ff7d5a1981" => :catalina
    sha256 "8b1752455e0ff26b804070e3eb710493342fc2b2897a132a26433f4cabf5ec17" => :mojave
    sha256 "c71f0b21d59a21fdc1e86e0a2016f79d862e838eb0fb7c92c50ed56e8aa1a163" => :high_sierra
    sha256 "3fc40e5d2ee26c7b8709bf61e651ec3506561b98fcbf6ca52b8d353dd4be356d" => :sierra
    sha256 "caa018741bb84409295f4fec33bcf427df199e717abf1323c9325d44238548ff" => :el_capitan
    sha256 "eb0a1df1375a29fd3a88cddbe844820c9650b4ee14406245ee5d93ad41e48586" => :yosemite
    sha256 "dfc0ad66122de0187db789cdafde75a367dc02748eede381567ca8f8a9208bde" => :mavericks
  end

  patch do
    url "https://deb.debian.org/debian/pool/main/i/iprint/iprint_1.3-9.diff.gz"
    sha256 "3a1ff260e6d639886c005ece754c2c661c0d3ad7f1f127ddb2943c092e18ab74"
  end

  def install
    system "make"
    bin.install "i"
    man1.install "i.1"
  end

  test do
    assert_equal shell_output("#{bin}/i 1234"), "1234 0x4D2 02322 0b10011010010\n"
  end
end
