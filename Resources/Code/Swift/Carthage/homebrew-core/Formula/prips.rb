class Prips < Formula
  desc "Print the IP addresses in a given range"
  homepage "https://devel.ringlet.net/sysutils/prips/"
  url "https://devel.ringlet.net/files/sys/prips/prips-1.1.1.tar.xz"
  sha256 "16efeac69b8bd9d13c80ec365ea66bc3bb8326dc23975acdac03184ee8da63a8"

  livecheck do
    url :homepage
    regex(/current version .*?prips.*?v?(\d+(?:\.\d+)+)/i)
  end

  bottle do
    cellar :any_skip_relocation
    sha256 "19470cbc2e9339887d983fe9e008a70c861cbe81c22c493bb83b9550d4204994" => :big_sur
    sha256 "500cfb4e4680b767d3a765e542e59660ed07d05724902e43d243be184194e916" => :arm64_big_sur
    sha256 "06f354f3564aa9aa391d56b952fb97056c911f32232f6afeefcb23bce5a8bc0c" => :catalina
    sha256 "771e030cbbf61a7914af375462d24bc2fccb6e60e8959110906e23544aacbb17" => :mojave
    sha256 "ecf0f743bfaffc303c8f520f5f29f10917b63708866fc50553c10f6952c5e06e" => :high_sierra
    sha256 "65a400f8d42e7c38cbc26898dadf3110b0aad7e347ba40585f398d4bcc696d04" => :sierra
  end

  def install
    system "make"
    bin.install "prips"
    man1.install "prips.1"
  end

  test do
    assert_equal "127.0.0.0\n127.0.0.1",
      shell_output("#{bin}/prips 127.0.0.0/31").strip
  end
end
