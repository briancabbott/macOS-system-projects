# Based on:
# Apple Open Source: https://opensource.apple.com/source/cvs/cvs-45/
# MacPorts: https://trac.macports.org/browser/trunk/dports/devel/cvs/Portfile
# Creating a useful testcase: https://mrsrl.stanford.edu/~brian/cvstutorial/

class Cvs < Formula
  desc "Version control system"
  homepage "https://www.nongnu.org/cvs/"
  url "https://ftp.gnu.org/non-gnu/cvs/source/feature/1.12.13/cvs-1.12.13.tar.bz2"
  sha256 "78853613b9a6873a30e1cc2417f738c330e75f887afdaf7b3d0800cb19ca515e"
  revision 2

  livecheck do
    url "https://ftp.gnu.org/non-gnu/cvs/source/feature/"
    regex(%r{href=.*?v?(\d+(?:\.\d+)+)/}i)
  end

  bottle do
    cellar :any_skip_relocation
    rebuild 1
    sha256 "0cf65c20d4220d636cfd9c9b4f4e6f3ab011fe01136e5677f9da56cb67c0c208" => :big_sur
    sha256 "2d6d9ac2f96edfbd55f9f13c215ba6aec3960c1c3f91e99294a512618b159bb9" => :catalina
    sha256 "c564cc0e316461844b51f36f9d13e357184af89c325edfe8c565fd1f74d6d2da" => :mojave
  end

  patch :p0 do
    url "https://opensource.apple.com/tarballs/cvs/cvs-45.tar.gz"
    sha256 "4d200dcf0c9d5044d85d850948c88a07de83aeded5e14fa1df332737d72dc9ce"
    apply "patches/PR5178707.diff",
          "patches/ea.diff",
          "patches/endian.diff",
          "patches/fixtest-client-20.diff",
          "patches/fixtest-recase.diff",
          "patches/i18n.diff",
          "patches/initgroups.diff",
          "patches/nopic.diff",
          "patches/remove-libcrypto.diff",
          "patches/remove-info.diff",
          "patches/tag.diff",
          "patches/zlib.diff"
  end

  # Fixes error: 'Illegal instruction: 4'; '%n used in a non-immutable format string' on 10.13
  # Patches the upstream-provided gnulib on all platforms as is recommended
  patch do
    url "https://raw.githubusercontent.com/Homebrew/formula-patches/24118ec737c7d008420d4683a07129ed80a759eb/cvs/vasnprintf-high-sierra-fix.diff"
    sha256 "affa485332f66bb182963680f90552937bf1455b855388f7c06ef6a3a25286e2"
  end

  # Fixes "cvs [init aborted]: cannot get working directory: No such file or directory" on Catalina.
  # Original patch idea by Jason White from stackoverflow
  patch :DATA

  def install
    # Work around configure issues with Xcode 12
    ENV.append "CFLAGS", "-Wno-implicit-function-declaration"

    system "./configure", "--disable-debug",
                          "--disable-dependency-tracking",
                          "--prefix=#{prefix}",
                          "--infodir=#{info}",
                          "--mandir=#{man}",
                          "--sysconfdir=#{etc}",
                          "--with-gssapi",
                          "--enable-pam",
                          "--enable-encryption",
                          "--with-external-zlib",
                          "--enable-case-sensitivity",
                          "--with-editor=vim",
                          "ac_cv_func_working_mktime=no"
    system "make"
    ENV.deparallelize
    system "make", "install"
  end

  test do
    cvsroot = testpath/"cvsroot"
    cvsroot.mkpath
    system "#{bin}/cvs", "-d", cvsroot, "init"

    mkdir "cvsexample" do
      ENV["CVSROOT"] = cvsroot
      system "#{bin}/cvs", "import", "-m", "dir structure", "cvsexample", "homebrew", "start"
    end
  end
end

__END__
--- cvs-1.12.13/lib/xgetcwd.c.orig      2019-10-10 22:52:37.000000000 -0500
+++ cvs-1.12.13/lib/xgetcwd.c   2019-10-10 22:53:32.000000000 -0500
@@ -25,8 +25,9 @@
 #include "xgetcwd.h"

 #include <errno.h>
+#include <unistd.h>

-#include "getcwd.h"
+/* #include "getcwd.h" */
 #include "xalloc.h"

 /* Return the current directory, newly allocated.
