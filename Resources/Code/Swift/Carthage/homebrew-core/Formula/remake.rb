class Remake < Formula
  desc "GNU Make with improved error handling, tracing, and a debugger"
  homepage "https://bashdb.sourceforge.io/remake"
  url "https://downloads.sourceforge.net/project/bashdb/remake/4.3%2Bdbg-1.5/remake-4.3%2Bdbg-1.5.tar.gz"
  version "4.3-1.5"
  sha256 "2e6eb709f3e6b85893f14f15e34b4c9b754aceaef0b92bb6ca3a025f10119d76"
  license "GPL-3.0-only"

  # We check the "remake" directory page because the bashdb project contains
  # various software and remake releases may be pushed out of the SourceForge
  # RSS feed.
  livecheck do
    url "https://sourceforge.net/projects/bashdb/files/remake/"
    strategy :page_match
    regex(%r{href=.*?remake/v?(\d+(?:\.\d+)+(?:(?:%2Bdbg)?[._-]\d+(?:\.\d+)+)?)/?["' >]}i)
  end

  bottle do
    rebuild 1
    sha256 "933b00f621a8cfc69a197d73bfe7f9d319d3571aae991eb3b039a8471ea9a0f1" => :big_sur
    sha256 "391321a2121b244a77d91ffb3ec32d039aa38445441bff436f6128164b51db16" => :arm64_big_sur
    sha256 "310b2ef02888a953487fb4e3f7fd7101c209a9abd12286d6a8509669c3ed2909" => :catalina
    sha256 "05998e7ad1f8442b57e0826b5152894186f359b59d75e68634c1da1a96b0345f" => :mojave
    sha256 "b3c14a7963aeda5e8367e0e4375354fdd58b24a99c07d6cb3fd881dc8d1b1941" => :high_sierra
  end

  depends_on "readline"

  conflicts_with "make", because: "both install texinfo files for make"

  def install
    system "./configure", "--disable-debug",
                          "--disable-dependency-tracking",
                          "--prefix=#{prefix}"
    system "make", "install"
  end

  test do
    (testpath/"Makefile").write <<~EOS
      all:
      \techo "Nothing here, move along"
    EOS
    system bin/"remake", "-x"
  end
end
