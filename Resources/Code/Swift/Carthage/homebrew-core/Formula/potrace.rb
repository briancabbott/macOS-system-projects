class Potrace < Formula
  desc "Convert bitmaps to vector graphics"
  homepage "https://potrace.sourceforge.io/"
  url "https://potrace.sourceforge.io/download/1.16/potrace-1.16.tar.gz"
  sha256 "be8248a17dedd6ccbaab2fcc45835bb0502d062e40fbded3bc56028ce5eb7acc"
  license "GPL-2.0"

  livecheck do
    url "http://potrace.sourceforge.net/"
    strategy :page_match
    regex(/href=.*?potrace[._-]v?(\d+(?:\.\d+)*)\.t/i)
  end

  bottle do
    cellar :any
    sha256 "3b5294deed86179a4e496236fb882eb0b7ad3c020741e2a1398861b545062712" => :big_sur
    sha256 "80d3d0256f9b7add7d3835f6c84f30afec6a4893f2fcd2aa44b07ebe95876c7f" => :arm64_big_sur
    sha256 "c3f357a8bd6460384400acd00dab0d8571ad0b1543a81e5b9d5ff49d1ece4fa1" => :catalina
    sha256 "3ad69cce4edecea6e5170b766201845b703a98bbac3c5272ef6a045f828643e2" => :mojave
    sha256 "56d821a4d3579bedf64ebf5357fc04f214cb2efbea7ddb681b202e684e71d97e" => :high_sierra
  end

  resource "head.pbm" do
    url "https://potrace.sourceforge.io/img/head.pbm"
    sha256 "3c8dd6643b43cf006b30a7a5ee9604efab82faa40ac7fbf31d8b907b8814814f"
  end

  def install
    system "./configure", "--disable-dependency-tracking",
                          "--prefix=#{prefix}",
                          "--mandir=#{man}",
                          "--with-libpotrace"
    system "make", "install"
  end

  test do
    resource("head.pbm").stage testpath
    system "#{bin}/potrace", "-o", "test.eps", "head.pbm"
    assert_predicate testpath/"test.eps", :exist?
  end
end
