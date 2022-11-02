class Ftgl < Formula
  desc "Freetype / OpenGL bridge"
  homepage "https://sourceforge.net/projects/ftgl/"
  url "https://downloads.sourceforge.net/project/ftgl/FTGL%20Source/2.1.3~rc5/ftgl-2.1.3-rc5.tar.gz"
  sha256 "5458d62122454869572d39f8aa85745fc05d5518001bcefa63bd6cbb8d26565b"

  livecheck do
    url :stable
    regex(%r{url=.*?/ftgl[._-]v?(\d+(?:\.\d+)+(?:-rc\d*)?)\.t}i)
  end

  bottle do
    cellar :any
    sha256 "1b39e663c0bedd0b915dd60c99c5f1acdfb3ae9717cd832134de15fd48736673" => :big_sur
    sha256 "85368ec5c37bb2cffd87cd30775a78956708c71749f8da56239fd93e57cf576d" => :arm64_big_sur
    sha256 "0f982773db625daa78a1c18c9e67315ede8c52850586d47d9b7a41ffcac91730" => :catalina
    sha256 "17370dd65d19b49dd8ca9d49d0da5010e5c5d1346122c46e0eec0c98b010fb13" => :mojave
    sha256 "f6da52f5e9f06f984aad457058876e88b5b7053288f40c87a17d7d5749936cd6" => :high_sierra
    sha256 "946a9530f7eae5c8f2bc71dfc91b3a8138ae2228cd441fd7cf39f047b957ce47" => :sierra
    sha256 "6462eb0b97ab120639f1a191f6e3a39419bbb813abd71f5c741303dbf0aed7fb" => :el_capitan
    sha256 "26db05485600adfb7ead23d04fae9b1ee1d1a4b7ac304e1453ad83b4b2c39f64" => :yosemite
    sha256 "50a41f3c95a363b52bc367abf4b5b9dc272d71c8b35fe8e63f058c7cf7162225" => :mavericks
  end

  depends_on "freetype"

  def install
    # If doxygen is installed, the docs may still fail to build.
    # So we disable building docs.
    inreplace "configure", "set dummy doxygen;", "set dummy no_doxygen;"

    system "./configure", "--disable-debug", "--disable-dependency-tracking",
                          "--prefix=#{prefix}",
                          "--disable-freetypetest",
                          # Skip building the example program by failing to find GLUT (MacPorts)
                          "--with-glut-inc=/dev/null",
                          "--with-glut-lib=/dev/null"

    system "make", "install"
  end
end
