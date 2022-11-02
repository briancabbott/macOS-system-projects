class GnuBarcode < Formula
  desc "Convert text strings to printed bars"
  homepage "https://www.gnu.org/software/barcode/"
  url "https://ftp.gnu.org/gnu/barcode/barcode-0.99.tar.gz"
  mirror "https://ftpmirror.gnu.org/barcode/barcode-0.99.tar.gz"
  sha256 "7c031cf3eb811242f53664379aebbdd9fae0b7b26b5e5d584c31a9f338154b64"
  license "GPL-3.0"

  livecheck do
    url :stable
  end

  bottle do
    cellar :any_skip_relocation
    sha256 "8db9cd7477dfce32af8a9451c792683d97ef0ab81d7929881ac59a6fab9d88aa" => :big_sur
    sha256 "8e621e1e142ab7c323b6bc133b4b3c373df050986e4282814f07a8ca1928d83c" => :arm64_big_sur
    sha256 "237ba00c7acb6a0343b17cae529d6a854ae321a03136d0f2882b010f4107230c" => :catalina
    sha256 "a24619af860a3658774fdcb5b5439ed751e1284b724e2f5dc8bb0c35736f879a" => :mojave
    sha256 "bd55ad14c9e7411d952d9243b6c4c7aa84162afe34ed1e3c3d9e9a368d2d6485" => :high_sierra
    sha256 "7588bb4800b5c348e103ed92e8bcc2f38812b8fbf4e254315e6429b3961e9f05" => :sierra
    sha256 "1885abad5bc70c2e9952e131307ca7282d851856ebdea58dadc69f0e125a7c22" => :el_capitan
    sha256 "819af5d364f041397c7c6b768829df7fcbd617f86194a1656b5523eeaed9415a" => :yosemite
    sha256 "285a9fa2833e843765087545f778aeadc670555bcac38193788c866826a88d42" => :mavericks
  end

  # Patch and ac_cv_func_calloc_0_nonnull config addresses the following issue:
  # https://lists.gnu.org/archive/html/bug-barcode/2015-06/msg00001.html
  patch :DATA

  def install
    system "./configure", "ac_cv_func_calloc_0_nonnull=yes",
                          "--disable-debug",
                          "--disable-dependency-tracking",
                          "--prefix=#{prefix}"

    system "make"
    system "make", "MAN1DIR=#{man1}",
                   "MAN3DIR=#{man3}",
                   "INFODIR=#{info}",
                   "install"
  end

  test do
    (testpath/"test.txt").write("12345")
    system "#{bin}/barcode", "-e", "CODE39", "-i", "test.txt", "-o", "test.ps"
    assert File.read("test.ps").start_with?("")
  end
end

__END__
diff -bur barcode-original/barcode.h barcode-new/barcode.h
--- barcode-original/barcode.h  2013-03-29 16:51:07.000000000 -0500
+++ barcode-new/barcode.h       2015-08-16 16:06:23.000000000 -0500
@@ -123,6 +123,6 @@
 }
 #endif

-int streaming;
+extern int streaming;

 #endif /* _BARCODE_H_ */
diff -bur barcode-original/pcl.c barcode-new/pcl.c
--- barcode-original/pcl.c      2013-03-29 17:23:31.000000000 -0500
+++ barcode-new/pcl.c   2015-08-16 16:07:06.000000000 -0500
@@ -29,6 +29,7 @@

 #define SHRINK_AMOUNT 0.15  /* shrink the bars to account for ink spreading */

+int streaming;

 /*
  * How do the "partial" and "textinfo" strings work? See file "ps.c"
