class Aldo < Formula
  desc "Morse code learning tool released under GPL"
  homepage "https://www.nongnu.org/aldo/"
  url "https://savannah.nongnu.org/download/aldo/aldo-0.7.7.tar.bz2"
  sha256 "f1b8849d09267fff3c1f5122097d90fec261291f51b1e075f37fad8f1b7d9f92"
  license "GPL-3.0"

  livecheck do
    url "https://download.savannah.gnu.org/releases/aldo/"
    regex(/href=.*?aldo[._-]v?(\d+(?:\.\d+)+)\.t/i)
  end

  bottle do
    cellar :any
    rebuild 1
    sha256 "b6e5c413c1ca391cd040cadd7d2c153e7552ce43677da0d0f1aab1467e92bb3b" => :big_sur
    sha256 "a0944cbf0df1cccd63c1d18cf758826e8b7621c188b60603153b717ba9d2edbf" => :arm64_big_sur
    sha256 "2a574bfd1a76ef4733d941234df142dfc87b05cefefaf58d0617113d7af85999" => :catalina
    sha256 "4c510b7da186be5d55c990d97265952de8fad51079ad2fa18058b8a57d8eeebb" => :mojave
    sha256 "d30e5e60defc2e2d2110cf52a60898d94ae3331a679f1c228e0d598421a594d9" => :high_sierra
    sha256 "ad5216c04fce4d1f4da63af2fa4d298a3414073db186991ec4389a942799ddd1" => :sierra
    sha256 "0691c4b9b7ae5b6f104c5b5205f731d4348563b8a9a8c3631395f619ce00aabf" => :el_capitan
    sha256 "f5d55cefcfc65033f50bf2aedb30298db1540a8dd5f5c028feb3b4b1c7e5610b" => :yosemite
    sha256 "fea59d120862f6a04da3993dde1b2f6db60183fc6d7f90f77bb622efdf8a16ac" => :mavericks
  end

  depends_on "libao"

  # Reported upstream:
  # https://savannah.nongnu.org/bugs/index.php?42127
  patch do
    url "https://raw.githubusercontent.com/Homebrew/formula-patches/85fa66a9/aldo/0.7.7.patch"
    sha256 "3b6c6cc067fc690b5af4042a2326cee2b74071966e9e2cd71fab061fde6c4a5d"
  end

  def install
    system "./configure", "--disable-dependency-tracking", "--prefix=#{prefix}"
    system "make", "install"
  end

  test do
    assert_match "Aldo #{version} Main Menu", pipe_output("#{bin}/aldo", "6")
  end
end
