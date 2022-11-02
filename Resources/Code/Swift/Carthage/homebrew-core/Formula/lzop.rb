class Lzop < Formula
  desc "File compressor"
  homepage "https://www.lzop.org/"
  url "https://dl.bintray.com/homebrew/mirror/lzop-1.04.tar.gz"
  mirror "https://www.lzop.org/download/lzop-1.04.tar.gz"
  sha256 "7e72b62a8a60aff5200a047eea0773a8fb205caf7acbe1774d95147f305a2f41"
  license "GPL-2.0"

  livecheck do
    url "https://www.lzop.org/download/"
    regex(/href=.*?lzop[._-]v?(\d+(?:\.\d+)+)\.t/i)
  end

  bottle do
    cellar :any
    sha256 "f04a876a2b69220632fb027546104d9c6cb9ee8bdb1d9f6a2845a054500d8bb7" => :big_sur
    sha256 "32390eb8141791296e84ed7c6a39edb5bb2ded9b04581c301d32a6dfa322db4d" => :arm64_big_sur
    sha256 "3aa57a50254d383c0fe0e4d0d0585e1525d50d0cd30f87390d087523348044a0" => :catalina
    sha256 "0ec93aa163500d45c456bce3ee100dbe61c4db080494ee41383286ca10f4d246" => :mojave
    sha256 "d42fafd3f1f39d9ab512f755bd216edd24002caf8a4da82f80818fe2c29f0556" => :high_sierra
    sha256 "73c2ce334be9317ca79509aec3acef2fa1eff0ffb69fdc10b3850b7f51101f72" => :sierra
    sha256 "26e49bf0d06fb60d7cd5c431634966f28993edc250c4d06b0db26b28aae3cd0d" => :el_capitan
    sha256 "d9e12c4bb51c43dd306d5283fde5c3350e3e1f7f1d48c05c831a57b058db1354" => :yosemite
  end

  depends_on "lzo"

  def install
    system "./configure", "--disable-dependency-tracking",
                          "--prefix=#{prefix}"
    system "make", "install"
  end

  test do
    path = testpath/"test"
    text = "This is Homebrew"
    path.write text

    system "#{bin}/lzop", "test"
    assert_predicate testpath/"test.lzo", :exist?
    rm path

    system "#{bin}/lzop", "-d", "test.lzo"
    assert_equal text, path.read
  end
end
