class Lha < Formula
  desc "Utility for creating and opening lzh archives"
  homepage "https://lha.osdn.jp/"
  # Canonical: https://osdn.net/dl/lha/lha-1.14i-ac20050924p1.tar.gz
  url "https://dotsrc.dl.osdn.net/osdn/lha/22231/lha-1.14i-ac20050924p1.tar.gz"
  version "1.14i-ac20050924p1"
  sha256 "b5261e9f98538816aa9e64791f23cb83f1632ecda61f02e54b6749e9ca5e9ee4"
  license "MIT"

  # OSDN releases pages use asynchronous requests to fetch the archive
  # information for each release, rather than including this information in the
  # page source. As such, we identify versions from the release names instead.
  # The portion of the regex that captures the version is looser than usual
  # because the version format is unusual and may change in the future.
  livecheck do
    url "https://osdn.net/projects/lha/releases/"
    regex(%r{href=.*?/projects/lha/releases/[^>]+?>\s*?v?(\d+(?:[.-][\da-z]+)+)}im)
  end

  bottle do
    cellar :any_skip_relocation
    rebuild 1
    sha256 "bd78eb55cbce8091fd07d82ec486bfd67fc8079b2fe6385c8374b2e7c5171528" => :big_sur
    sha256 "d328d1b1740353a2e04c6f79dc863f3fa2caca9380e76b3e48b4b72f5e1ad32b" => :arm64_big_sur
    sha256 "429d3165a0f986e815f09ea3f6b2d93e1bd0feef01b6df6159a983e8118244a4" => :catalina
    sha256 "12b5c79de56f71138c64d517ffc0091bc313f4cc0f174e10276b248b06e2fa0f" => :mojave
  end

  head do
    url "https://github.com/jca02266/lha.git"
    depends_on "autoconf" => :build
    depends_on "automake" => :build
  end

  conflicts_with "lhasa", because: "both install a `lha` binary"

  def install
    # Work around configure/build issues with Xcode 12
    ENV.append "CFLAGS", "-Wno-implicit-function-declaration"

    system "autoreconf", "-is" if build.head?
    system "./configure", "--disable-debug",
                          "--disable-dependency-tracking",
                          "--prefix=#{prefix}",
                          "--mandir=#{man}"
    system "make", "install"
  end

  test do
    (testpath/"foo").write "test"
    system "#{bin}/lha", "c", "foo.lzh", "foo"
    assert_equal "::::::::\nfoo\n::::::::\ntest",
      shell_output("#{bin}/lha p foo.lzh")
  end
end
