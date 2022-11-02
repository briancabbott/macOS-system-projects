class Libuecc < Formula
  desc "Very small Elliptic Curve Cryptography library"
  homepage "https://git.universe-factory.net/libuecc/"
  url "https://git.universe-factory.net/libuecc/snapshot/libuecc-7.tar"
  sha256 "0120aee869f56289204255ba81535369816655264dd018c63969bf35b71fd707"
  head "https://git.universe-factory.net/libuecc", using: :git

  livecheck do
    url :head
    regex(/href=.*?libuecc[._-]v?(\d+(?:\.\d+)*)\.t/i)
  end

  bottle do
    cellar :any
    sha256 "844327a3e5e6bed43c2ed9a36e3b7f6c8c871803fb5968f34ee6aa667fc345b8" => :big_sur
    sha256 "411158650719304f490887eb4a88d54a6a10ccee7238c7f7a92fb5407c312813" => :arm64_big_sur
    sha256 "89acc7a04f910882b89d9e032a45e8c27dc98257d6d4e6b28f6c6a26c8c369ae" => :catalina
    sha256 "d4d0c41262688ddca9ee2f2e6b80c33670c5a8db7266cd0c0592cd50b0d18be1" => :mojave
    sha256 "95646c23acf19c1f07032c6f311f446e7a32b1a9d0c1dd385ec3c41811036572" => :high_sierra
    sha256 "4722877fdc4538c814a10e6d0dc2f1a4d2a3571ce4ca1c8b37279c88cd83883f" => :sierra
    sha256 "d9e52027a6535fb74e44026d23ef13a2417a1f22402173dc90d136071ea5290d" => :el_capitan
  end

  depends_on "cmake" => :build

  def install
    system "cmake", ".", *std_cmake_args
    system "make", "install"
  end

  test do
    (testpath/"test.c").write <<~EOS
      #include <stdlib.h>
      #include <libuecc/ecc.h>

      int main(void)
      {
          ecc_int256_t secret;
          ecc_25519_gf_sanitize_secret(&secret, &secret);

          return EXIT_SUCCESS;
      }
    EOS
    system ENV.cc, "-I#{include}/libuecc-#{version}", "-L#{lib}", "-o", "test", "test.c", "-luecc"
    system "./test"
  end
end
