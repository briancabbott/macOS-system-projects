class Libedit < Formula
  desc "BSD-style licensed readline alternative"
  homepage "https://thrysoee.dk/editline/"
  url "https://thrysoee.dk/editline/libedit-20191231-3.1.tar.gz"
  version "20191231-3.1"
  sha256 "dbb82cb7e116a5f8025d35ef5b4f7d4a3cdd0a3909a146a39112095a2d229071"

  livecheck do
    url :homepage
    regex(/href=.*?libedit[._-]v?(\d{4,}-\d+(?:\.\d+)+)\.t/i)
  end

  bottle do
    cellar :any
    sha256 "7add4831a5be1d9829064a690c36fb47a9a3b75e8a59acf266f4fc4f2a3ad4f6" => :big_sur
    sha256 "9150af78748811901eee0d2e3c5199ed14dd7722a466d6fb9b1d900369e3e86f" => :arm64_big_sur
    sha256 "faa58f2e587c5b982af44765f7a034a27837fc1e94816e094ace3f408ab4a7bf" => :catalina
    sha256 "a707377be9d5fef881cdbb77ad3b562c9d5f54befb97a10d0b7158e4db87ef86" => :mojave
    sha256 "06e087927f024a9030947216be3aaa46f97fc9dcc1b70959f60240b86bd8f574" => :high_sierra
  end

  keg_only :provided_by_macos

  uses_from_macos "ncurses"

  def install
    system "./configure", "--disable-dependency-tracking",
                          "--disable-silent-rules",
                          "--prefix=#{prefix}"
    system "make", "install"
  end

  test do
    (testpath/"test.c").write <<~EOS
      #include <stdio.h>
      #include <histedit.h>
      int main(int argc, char *argv[]) {
        EditLine *el = el_init(argv[0], stdin, stdout, stderr);
        return (el == NULL);
      }
    EOS
    system ENV.cc, "test.c", "-o", "test", "-L#{lib}", "-ledit", "-I#{include}"
    system "./test"
  end
end
