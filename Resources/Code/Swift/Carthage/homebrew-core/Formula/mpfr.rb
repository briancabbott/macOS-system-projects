class Mpfr < Formula
  desc "C library for multiple-precision floating-point computations"
  homepage "https://www.mpfr.org/"
  url "https://ftp.gnu.org/gnu/mpfr/mpfr-4.1.0.tar.xz"
  mirror "https://ftpmirror.gnu.org/mpfr/mpfr-4.1.0.tar.xz"
  sha256 "0c98a3f1732ff6ca4ea690552079da9c597872d30e96ec28414ee23c95558a7f"
  license "GPL-3.0"

  livecheck do
    url :stable
  end

  bottle do
    cellar :any
    sha256 "1e8eb0326f62d3461d420d98af6fc088daca481cae89fd77a75b420d2e76d776" => :big_sur
    sha256 "9df11560dd3650ffae35c134cef6e0e91aad0e862f5c8895c568b828cf0598d5" => :arm64_big_sur
    sha256 "5fcf57834f58c18761c6c7b0eb961eb7f9fc54325b5361bf3a17c4dee6ebc08a" => :catalina
    sha256 "93c0d2ca093819f125300002cd34c1d1b4dfb7a1403729205861bec21388ff12" => :mojave
    sha256 "77581a1df66fb1ef55ffb19777d08b0b60fbc3d2d7ad491a8aceb3a6a4bf7ffd" => :high_sierra
  end

  depends_on "gmp"

  def install
    system "./configure", "--disable-dependency-tracking", "--prefix=#{prefix}",
                          "--disable-silent-rules"
    system "make"
    system "make", "check"
    system "make", "install"
  end

  test do
    (testpath/"test.c").write <<~EOS
      #include <mpfr.h>
      #include <math.h>
      #include <stdlib.h>

      int main() {
        mpfr_t x, y;
        mpfr_inits2 (256, x, y, NULL);
        mpfr_set_ui (x, 2, MPFR_RNDN);
        mpfr_root (y, x, 2, MPFR_RNDN);
        mpfr_pow_si (x, y, 4, MPFR_RNDN);
        mpfr_add_si (y, x, -4, MPFR_RNDN);
        mpfr_abs (y, y, MPFR_RNDN);
        if (fabs(mpfr_get_d (y, MPFR_RNDN)) > 1.e-30) abort();
        return 0;
      }
    EOS
    system ENV.cc, "test.c", "-L#{lib}", "-L#{Formula["gmp"].opt_lib}",
                   "-lgmp", "-lmpfr", "-o", "test"
    system "./test"
  end
end
