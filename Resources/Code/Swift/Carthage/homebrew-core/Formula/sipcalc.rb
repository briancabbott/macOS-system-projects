class Sipcalc < Formula
  desc "Advanced console-based IP subnet calculator"
  homepage "https://www.routemeister.net/projects/sipcalc/"
  url "https://www.routemeister.net/projects/sipcalc/files/sipcalc-1.1.6.tar.gz"
  sha256 "cfd476c667f7a119e49eb5fe8adcfb9d2339bc2e0d4d01a1d64b7c229be56357"

  livecheck do
    url "https://www.routemeister.net/projects/sipcalc/download.html"
    regex(/href=.*?sipcalc[._-]v?(\d+(?:\.\d+)+)\.t/i)
  end

  bottle do
    cellar :any_skip_relocation
    sha256 "7ecd4de1c66d06136e36ec83e076b253212294f0407bf049e1bdf7746505c2ab" => :big_sur
    sha256 "9eb0d11f79f4a89148dcfba3ff714cad9c345276ce5ca0e8e937782cbc0d0e1d" => :arm64_big_sur
    sha256 "4b211b4978bd165adb71435e19f19f146ee84f905555c3bce2d4652375067d3d" => :catalina
    sha256 "50bc96758ca5ecdb86fb29ca39bf07f6c4e44192310481436afccc191c6f2cd2" => :mojave
    sha256 "9cff165f5e2b98d0c7d4729d4d6309b679cae7d161996242c666053d37134640" => :high_sierra
    sha256 "1ccdaec0a816dde9f7caa0f7a77cd984ece78a61a5886032c4c8821915753482" => :sierra
    sha256 "56aa686252ac703ed3dbe91f5737ec4d4b95d52516f4ab52947df15b77d1c58f" => :el_capitan
    sha256 "6b2fc300755693d382fd5ea971c272a7c8c7bff49614dd88d8db4270aa496012" => :yosemite
    sha256 "7ddf7b200984de97143828faf6385314a2ff3f4436432d810e5aaf7dfe44e78c" => :mavericks
  end

  def install
    system "./configure", "--disable-debug",
                          "--disable-dependency-tracking",
                          "--prefix=#{prefix}"
    system "make", "install"
  end

  test do
    system "#{bin}/sipcalc", "-h"
  end
end
