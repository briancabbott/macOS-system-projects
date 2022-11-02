class Libsquish < Formula
  desc "Library for compressing images with the DXT standard"
  homepage "https://sourceforge.net/projects/libsquish/"
  url "https://downloads.sourceforge.net/project/libsquish/libsquish-1.15.tgz"
  sha256 "628796eeba608866183a61d080d46967c9dda6723bc0a3ec52324c85d2147269"
  license "MIT"

  livecheck do
    url :stable
  end

  bottle do
    cellar :any_skip_relocation
    sha256 "bfc3e9d3b6239828025e5a35707ac35a82722c9a1f66b13116990a3597f33943" => :big_sur
    sha256 "a3f9fc5e20792dd4ced369a1be063a01c873afc399c8c73bb8800db1777ce6fb" => :catalina
    sha256 "d3a42b4342fab6548ec4e2467a571631edf0891d1c4c51a6573b14afda5b0972" => :mojave
    sha256 "734574ea8d63c2d52f291c1d3c96e18ee7fb6f404b5039245a31625fcb6277df" => :high_sierra
    sha256 "ebfa2b4d94a71334548800ceb00803ed1ed1e91226f6892048f376d73ee7ef74" => :sierra
    sha256 "4af6195448040889de7ada48fcb6fc6dd945e47f001a04807b70b4f5b3982663" => :el_capitan
    sha256 "d887794fa29f03abcd3809db6ea74045d3b8d40d895cf5972d2eda3de86f3ada" => :yosemite
  end

  depends_on "cmake" => :build

  def install
    system "cmake", ".", *std_cmake_args
    system "make", "install"
  end

  test do
    (testpath/"test.cc").write <<~EOS
      #include <stdio.h>
      #include <squish.h>
      int main(void) {
        printf("%d", GetStorageRequirements(640, 480, squish::kDxt1));
        return 0;
      }
    EOS
    system ENV.cxx, "-o", "test", "test.cc", lib/"libsquish.a"
    assert_equal "153600", shell_output("./test")
  end
end
