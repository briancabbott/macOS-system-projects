class Advancemame < Formula
  desc "MAME with advanced video support"
  homepage "https://www.advancemame.it/"
  url "https://github.com/amadvance/advancemame/releases/download/v3.9/advancemame-3.9.tar.gz"
  sha256 "3e4628e1577e70a1dbe104f17b1b746745b8eda80837f53fbf7b091c88be8c2b"
  license "GPL-2.0"

  livecheck do
    url :stable
    strategy :github_latest
  end

  bottle do
    sha256 "b04cdb0a02ef28b8626eb92c9da9ae784e7f7ba7f6ab8675cd40e5d976e46228" => :big_sur
    sha256 "fe56e046756c3f764cbe72137877c3bde3fd157175fc2347ae85874f9c2abe2b" => :arm64_big_sur
    sha256 "7530ca2e37ac45b53164ae54ab6669f1796ea0af88541a85a93a74c155fb0029" => :catalina
    sha256 "95f2cdff91ff98c3c9f65a0751d7948cefb3829d96e1977b5b8869163eba0790" => :mojave
    sha256 "9c5e0a9f81f43ec02eb951b82b4930639dafcdbacbeadf7bcc5e74f2e2ec7c45" => :high_sierra
    sha256 "6ba2c02db07a9ef7ddf561dbc2cde3904abe0b1b0ab9c9469020fdeced72e011" => :sierra
  end

  depends_on "pkg-config" => :build
  depends_on "freetype"
  depends_on "sdl"

  uses_from_macos "expat"
  uses_from_macos "ncurses"

  conflicts_with "advancemenu", because: "both install `advmenu` binaries"

  def install
    system "./configure", "--prefix=#{prefix}"
    system "make", "install", "LDFLAGS=#{ENV.ldflags}", "mandir=#{man}", "docdir=#{doc}"
  end

  test do
    system "#{bin}/advmame", "--version"
  end
end
