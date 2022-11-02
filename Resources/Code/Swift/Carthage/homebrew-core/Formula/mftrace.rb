class Mftrace < Formula
  desc "Trace TeX bitmap font to PFA, PFB, or TTF font"
  homepage "https://lilypond.org/mftrace/"
  url "https://lilypond.org/downloads/sources/mftrace/mftrace-1.2.20.tar.gz"
  mirror "https://dl.bintray.com/homebrew/mirror/mftrace-1.2.20.tar.gz"
  sha256 "626b7a9945a768c086195ba392632a68d6af5ea24ef525dcd0a4a8b199ea5f6f"
  license "GPL-2.0"
  revision 1

  bottle do
    cellar :any_skip_relocation
    sha256 "09ca3daeb696824e12655d6dbd0c768a0dcc07306c35001bc7b90417fb2b38c6" => :big_sur
    sha256 "da5fc2002936d2260121ce8134472bce14e8bccfb406fe9e1e56591037aa9751" => :catalina
    sha256 "d4b3535bdd69a89c59c4b7d7011ccb06544108c376e6313f62062c32991dece2" => :mojave
    sha256 "e1d8b241eb03982520cf2b4b2f8794fe74afb240247e4ea7c8164b1c9a22e974" => :high_sierra
  end

  head do
    url "https://github.com/hanwen/mftrace.git"
    depends_on "autoconf" => :build
  end

  depends_on "fontforge"
  depends_on "potrace"
  depends_on "python@3.9"
  depends_on "t1utils"

  # Fixed in https://github.com/hanwen/mftrace/pull/14
  resource "manpage" do
    url "https://github.com/hanwen/mftrace/raw/release/1.2.20/gf2pbm.1"
    sha256 "f2a7234cba5f59237e3cc1f67e395046b381a012456d4e6e9963673cf35d46fb"
  end

  def install
    buildpath.install resource("manpage") if build.stable?
    system "./autogen.sh" if build.head?
    system "./configure", "--prefix=#{prefix}"
    system "make", "install"
  end

  test do
    system "#{bin}/mftrace", "--version"
  end
end
