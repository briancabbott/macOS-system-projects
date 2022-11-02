class Fox < Formula
  desc "Toolkit for developing Graphical User Interfaces easily"
  homepage "http://www.fox-toolkit.org/"
  url "http://fox-toolkit.org/ftp/fox-1.6.56.tar.gz"
  sha256 "c517e5fcac0e6b78ca003cc167db4f79d89e230e5085334253e1d3f544586cb2"
  license "LGPL-2.1-or-later"
  revision 2

  livecheck do
    url "http://www.fox-toolkit.org/news.html"
    regex(/FOX STABLE v?(\d+(?:\.\d+)+)/i)
  end

  bottle do
    cellar :any
    sha256 "f7988beb83a1343a270ba6107f8693550fb4b6f92632600849eb11f203bfa2fc" => :big_sur
    sha256 "9e595940c212b8efb8588736216000490c8e8f4eff89b96be34aa92702538f1f" => :arm64_big_sur
    sha256 "e9f946383a4fc88a230622abd2c38386053f20c35eb632bf62ea8e06e43be7ab" => :catalina
    sha256 "7017807cda0f8aa8e43338d4556ec842db95626984f7a9eaef4b926a9dff7310" => :mojave
    sha256 "3705392848b062aa09d8be70c0f99b0331eeeceaea685389d684644e86f7fe22" => :high_sierra
  end

  depends_on "fontconfig"
  depends_on "freetype"
  depends_on "jpeg"
  depends_on "libpng"
  depends_on "libtiff"
  depends_on "libx11"
  depends_on "libxcursor"
  depends_on "libxext"
  depends_on "libxfixes"
  depends_on "libxft"
  depends_on "libxi"
  depends_on "libxrandr"
  depends_on "libxrender"
  depends_on "mesa"
  depends_on "mesa-glu"

  def install
    # Needed for libxft to find ftbuild2.h provided by freetype
    ENV.append "CPPFLAGS", "-I#{Formula["freetype"].opt_include}/freetype2"
    system "./configure", "--disable-dependency-tracking",
                          "--enable-release",
                          "--prefix=#{prefix}",
                          "--with-x",
                          "--with-opengl"
    # Unset LDFLAGS, "-s" causes the linker to crash
    system "make", "install", "LDFLAGS="
    rm bin/"Adie.stx"
  end

  test do
    system bin/"reswrap", "-t", "-o", "text.txt", test_fixtures("test.jpg")
    assert_match "\\x00\\x85\\x80\\x0f\\xae\\x03\\xff\\xd9", File.read("text.txt")
  end
end
