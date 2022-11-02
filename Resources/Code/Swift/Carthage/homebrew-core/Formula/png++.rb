class Pngxx < Formula
  desc "C++ wrapper for libpng library"
  homepage "https://www.nongnu.org/pngpp/"
  url "https://download.savannah.gnu.org/releases/pngpp/png++-0.2.10.tar.gz"
  sha256 "998af216ab16ebb88543fbaa2dbb9175855e944775b66f2996fc945c8444eee1"

  livecheck do
    url "https://download.savannah.gnu.org/releases/pngpp/"
    regex(/href=.*?png\+\+[._-]v?(\d+(?:\.\d+)+)\.t/i)
  end

  bottle do
    cellar :any_skip_relocation
    sha256 "536874bdcfa9f6b546f8a924bf4b72b8b6beba84883e6ee93645080632b51a2e" => :big_sur
    sha256 "4ff35d41f1ad4d5cff20cc77fa72c905a6cab65f0102dfe898241082ffc53b24" => :arm64_big_sur
    sha256 "c6377c5185e7ae53ff7ec9a133b8c12618a400f64d14b55ee751dc7c85cbc491" => :catalina
    sha256 "536f9c2dd05cfd2ae8a4f7f5d0c5c38575cf91609498f98bd6c3f97c4de2c520" => :mojave
    sha256 "536f9c2dd05cfd2ae8a4f7f5d0c5c38575cf91609498f98bd6c3f97c4de2c520" => :high_sierra
    sha256 "cee110f568bae723e8e5172e8bab36c8f4c5adb8bf339a444926a572bfa13f89" => :sierra
  end

  depends_on "libpng"

  def install
    system "make", "PREFIX=#{prefix}", "install"
  end

  test do
    (testpath/"test.cpp").write <<~EOS
      #include <png++/png.hpp>
      int main() {
        png::image<png::rgb_pixel> image(200, 300);
        if (image.get_width() != 200) return 1;
        if (image.get_height() != 300) return 2;
        return 0;
      }
    EOS
    system ENV.cxx, "test.cpp", "-o", "test"
    system "./test"
  end
end
