class Logstalgia < Formula
  desc "Web server access log visualizer with retro style"
  homepage "https://logstalgia.io/"
  url "https://github.com/acaudwell/Logstalgia/releases/download/logstalgia-1.1.2/logstalgia-1.1.2.tar.gz"
  sha256 "ed3f4081e401f4a509761a7204bdbd7c34f8f1aff9dcb030348885fb3995fca9"
  license "GPL-3.0"
  revision 1

  bottle do
    sha256 "8317c3e8cc8d1ae6d10457ccb7b2fb4d9add7b7b8b208dc70fccd49c556213d8" => :big_sur
    sha256 "9c8e8ae7c6d2fecce41f7ee986b0070c00abcc26a9ede7c0a89710e3921e73e9" => :arm64_big_sur
    sha256 "e292916be0cc939d985c4f42930d5217cf06d1e57fa2a3e376d55a44c4b21fd9" => :catalina
    sha256 "ecc61da046585777d74c682a14f6e3963570603188d2d447d3fbc4c5f87895dd" => :mojave
    sha256 "c0411062c997c5ca8aaf27726d2205601438a50ccbecc9a166c26c30bd3c08aa" => :high_sierra
  end

  head do
    url "https://github.com/acaudwell/Logstalgia.git"

    depends_on "autoconf" => :build
    depends_on "automake" => :build
    depends_on "libtool" => :build
  end

  depends_on "boost" => :build
  depends_on "glm" => :build
  depends_on "pkg-config" => :build
  depends_on "freetype"
  depends_on "glew"
  depends_on "libpng"
  depends_on "pcre"
  depends_on "sdl2"
  depends_on "sdl2_image"

  def install
    # clang on Mt. Lion will try to build against libstdc++,
    # despite -std=gnu++0x
    ENV.libcxx

    # For non-/usr/local installs
    ENV.append "CXXFLAGS", "-I#{HOMEBREW_PREFIX}/include"

    # Handle building head.
    system "autoreconf", "-f", "-i" if build.head?

    system "./configure", "--disable-dependency-tracking",
                          "--prefix=#{prefix}",
                          "--without-x"
    system "make"
    system "make", "install"
  end

  test do
    assert_match "Logstalgia v1.", shell_output("#{bin}/logstalgia --help")
  end
end
