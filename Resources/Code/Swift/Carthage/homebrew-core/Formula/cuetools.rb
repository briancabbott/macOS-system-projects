class Cuetools < Formula
  desc "Utilities for .cue and .toc files"
  homepage "https://github.com/svend/cuetools"
  url "https://github.com/svend/cuetools/archive/1.4.1.tar.gz"
  sha256 "24a2420f100c69a6539a9feeb4130d19532f9f8a0428a8b9b289c6da761eb107"
  license "GPL-2.0"
  head "https://github.com/svend/cuetools.git"

  bottle do
    cellar :any_skip_relocation
    sha256 "f649575d3661f08e573a8e72aa3e45580ed8f75f89fbe2409b2684732ab0f0a3" => :big_sur
    sha256 "d6065775e7e6286464caa093613104c747720e1b9ea98f29f71abea5a365ac05" => :arm64_big_sur
    sha256 "dc2d7bfcb8fd048421265da986fdb381007d64c7d2a45d45a53b896bad78bf18" => :catalina
    sha256 "1e36c3c8d2d53947b73a9f0a0aed74145e2b1890f83764de02f1d12566d0300f" => :mojave
    sha256 "4393d6db857a9568a34de3a09ff049fbec9a55a95b029eacd24e35d6ce792074" => :high_sierra
    sha256 "9456e5957a78f993f5a8cef76aa583ac6a42a8298fb05bded243dbaf810f9a44" => :sierra
    sha256 "7f0effc75d64fca0f2695b5f7ddb4d8713cc83522d40dcd37842e83c120ac117" => :el_capitan
    sha256 "81d06ef2e3d98061f332a535b810102c1be0505371c1ac1aed711cf2ae8de5a3" => :yosemite
    sha256 "95216c0df3840b2602e61dd3bef7d4c9b65cec0315e5b23ac87329320d9f6be9" => :mavericks
  end

  depends_on "autoconf" => :build
  depends_on "automake" => :build
  depends_on "libtool" => :build

  # see https://github.com/svend/cuetools/pull/18
  patch :DATA

  def install
    system "autoreconf", "-i"
    system "./configure", "--prefix=#{prefix}", "--mandir=#{man}"
    system "make", "install"
  end

  test do
    (testpath/"test.cue").write <<~EOS
      FILE "sampleimage.bin" BINARY
        TRACK 01 MODE1/2352
          INDEX 01 00:00:00
    EOS
    system "cueconvert", testpath/"test.cue", testpath/"test.toc"
    assert_predicate testpath/"test.toc", :exist?
  end
end

__END__
diff --git a/configure.ac b/configure.ac
index f54bb92..84ab467 100644
--- a/configure.ac
+++ b/configure.ac
@@ -1,5 +1,5 @@
 AC_INIT([cuetools], [1.4.0], [svend@ciffer.net])
-AM_INIT_AUTOMAKE([-Wall -Werror foreign])
+AM_INIT_AUTOMAKE([-Wall -Werror -Wno-extra-portability foreign])
 AC_PROG_CC
 AC_PROG_INSTALL
 AC_PROG_RANLIB
