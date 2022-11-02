class GnuShogi < Formula
  desc "Japanese Chess"
  homepage "https://www.gnu.org/software/gnushogi/"
  url "https://ftp.gnu.org/gnu/gnushogi/gnushogi-1.4.2.tar.gz"
  mirror "https://ftpmirror.gnu.org/gnushogi/gnushogi-1.4.2.tar.gz"
  sha256 "1ecc48a866303c63652552b325d685e7ef5e9893244080291a61d96505d52b29"
  license "GPL-3.0"

  livecheck do
    url :stable
  end

  bottle do
    sha256 "70258434181a6f40b0c3cddb7e2a5f0119bf953bff5dbd3e795533f558a104ea" => :big_sur
    sha256 "106fee874d8adf30ee887dcf7aa6149cd469c3e629861d105a278a9a66318aea" => :arm64_big_sur
    sha256 "6c559fdfcd24543c1f83f681fe3337048783d17649804b642fb0063dee88d7c8" => :catalina
    sha256 "c52d5743a6b9b6aeff9ba4b87104fa7adb58e7752683420e2c038f0216a2447d" => :mojave
    sha256 "20895a9d3fe87357df4dad1aaae16fee4d7a0c70e95119756c8ab2928817c161" => :high_sierra
    sha256 "677531c9eb7bdd01f22862c24d5ab144f7b78bd672223854fc169d103a9924e2" => :sierra
    sha256 "49ff431036e172362b24dc7eca426a638ec2953ea014c67e4cae239e9175bf27" => :el_capitan
    sha256 "1fbc9bf567ea4c50c5bb12fda953e18ce8617298d474ed8a56ca2b9dd24b2726" => :yosemite
  end

  def install
    system "./configure", "--prefix=#{prefix}"
    system "make"
    system "make", "install", "MANDIR=#{man6}", "INFODIR=#{info}"
  end

  test do
    (testpath/"test").write <<~EOS
      7g7f
      exit
    EOS
    system "#{bin}/gnushogi < test"
  end
end
