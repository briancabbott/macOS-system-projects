class Camellia < Formula
  desc "Image Processing & Computer Vision library written in C"
  homepage "https://camellia.sourceforge.io/"
  url "https://downloads.sourceforge.net/project/camellia/Unix_Linux%20Distribution/v2.7.0/CamelliaLib-2.7.0.tar.gz"
  sha256 "a3192c350f7124d25f31c43aa17e23d9fa6c886f80459cba15b6257646b2f3d2"

  livecheck do
    url :stable
    regex(%r{url=.*?/CamelliaLib[._-]v?(\d+(?:\.\d+)+)\.t}i)
  end

  bottle do
    cellar :any
    sha256 "84ce9367fd905515a5532cd64be374177b369f8c1797808a2ec95b5c89799965" => :big_sur
    sha256 "ecd83455b65819e9275ead160b6fca0a1a13e8b85d00c63e394ecdb5818b3a78" => :arm64_big_sur
    sha256 "c7d2e77a15331cebfeff928b67bd32ee5b0a9325ac5cbea022b2c6ddbe585ff6" => :catalina
    sha256 "347284dc085d1cd6acad286e8797ba3e001190e7cb04934b1f96d1e67481f302" => :mojave
    sha256 "fc8cb8a0f24226fd1f93b32192f290107d44283196e1edb48458b184597aa729" => :high_sierra
    sha256 "b4783ca8cf782a63d09daa1ff363c2fb4c4ea6dd4e75b8beb29167f536227730" => :sierra
    sha256 "a80b2f52fd6811c5c4017bceac418d241c30342c93c1e9ae8911ed5274630e9c" => :el_capitan
    sha256 "94196d40772f262cedb88f3dcf8b66c84fcc78cd419b439bd9619c25d602c8b1" => :yosemite
    sha256 "73db73665d4a3972bc5c0b6250d3bc050de83e54330c88e9282b970bf5ececce" => :mavericks
  end

  def install
    system "./configure", "--disable-debug", "--disable-dependency-tracking",
                          "--prefix=#{prefix}"
    system "make", "install"
  end

  test do
    (testpath/"test.cpp").write <<~EOS
      #include "camellia.h"
      int main() {
        CamImage image; // CamImage is an internal structure of Camellia
        return 0;
      }
    EOS

    system ENV.cc, "-I#{include}", "-L#{lib}", "-lcamellia", "-o", "test", "test.cpp"
    system "./test"
  end
end
