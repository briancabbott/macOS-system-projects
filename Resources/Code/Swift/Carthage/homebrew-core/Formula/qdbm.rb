class Qdbm < Formula
  desc "Library of routines for managing a database"
  homepage "https://fallabs.com/qdbm/"
  url "https://fallabs.com/qdbm/qdbm-1.8.78.tar.gz"
  sha256 "b466fe730d751e4bfc5900d1f37b0fb955f2826ac456e70012785e012cdcb73e"
  license "LGPL-2.1"

  bottle do
    cellar :any
    rebuild 1
    sha256 "7257a9e22ee3661fc2213d5ff60148b44e5e217781a3af807405c239020b3c6a" => :big_sur
    sha256 "5b0f851a602c8cb4f0fab49204037f7a6d28bc311a30559c7f08c37c36b66add" => :arm64_big_sur
    sha256 "0a0ba32270742fbd821ba60bbc6452e6b6b6a476d72e719bdb33fdf535e316f0" => :catalina
    sha256 "4861035c21a7fcd02efca60c922d06a45f3078eaffa374784a533932f9efa806" => :mojave
    sha256 "4ec4e60b16efb21fd7835c182fcf5d8f43c4af4329dd8afb07b4900bc1b17f60" => :high_sierra
    sha256 "547ecf82252706d276c8359448b7f4e738264999028b06cd3738af34ba58276c" => :sierra
    sha256 "6fd80b953a53cdf048bf686d2ac3620deda19a022a10a1e7cbd7aea073bf9b6a" => :el_capitan
    sha256 "4784d30c880c089dcef588c7d91d537269404a4917c9b2b1ef8b5123a727cee1" => :yosemite
    sha256 "bf5c5c1a087e22f9f06d29e2e139e55f6866ac1826ef725733d108ace6cf4d67" => :mavericks
  end

  def install
    system "./configure", "--disable-debug",
                          "--prefix=#{prefix}",
                          "--enable-bzip",
                          "--enable-zlib",
                          "--enable-iconv"
    on_macos do
      system "make", "mac"
      system "make", "check-mac"
      system "make", "install-mac"
    end
    on_linux do
      system "make"
      system "make", "check"
      system "make", "install"
    end
  end

  test do
    (testpath/"test.c").write <<~EOS
      #include <depot.h>
      #include <stdlib.h>
      #include <stdio.h>

      #define NAME     "mike"
      #define NUMBER   "00-12-34-56"
      #define DBNAME   "book"

      int main(void) {
        DEPOT *depot;
        char *val;

        if(!(depot = dpopen(DBNAME, DP_OWRITER | DP_OCREAT, -1))) { return 1; }
        if(!dpput(depot, NAME, -1, NUMBER, -1, DP_DOVER)) { return 1; }
        if(!(val = dpget(depot, NAME, -1, 0, -1, NULL))) { return 1; }

        printf("%s, %s\\n", NAME, val);
        free(val);

        if(!dpclose(depot)) { return 1; }

        return 0;
      }
    EOS

    system ENV.cc, "test.c", "-I#{include}", "-L#{lib}", "-lqdbm", "-o", "test"
    assert_equal "mike, 00-12-34-56", shell_output("./test").chomp
  end
end
