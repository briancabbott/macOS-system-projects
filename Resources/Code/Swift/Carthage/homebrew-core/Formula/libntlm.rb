class Libntlm < Formula
  desc "Implements Microsoft's NTLM authentication"
  homepage "https://www.nongnu.org/libntlm/"
  url "https://www.nongnu.org/libntlm/releases/libntlm-1.6.tar.gz"
  sha256 "f2376b87b06d8755aa3498bb1226083fdb1d2cf4460c3982b05a9aa0b51d6821"
  license "LGPL-2.1"

  livecheck do
    url "https://www.nongnu.org/libntlm/releases/"
    regex(/href=.*?libntlm[._-]v?(\d+(?:\.\d+)+)\.t/i)
  end

  bottle do
    cellar :any
    sha256 "c8662388829c4cacb52f1c948b8ae2ba19ad8c28ef53cda651b0e5cf8c0529fe" => :big_sur
    sha256 "0b3b553a37a15dd0ffd396e8d07b295197e39f8f4e8670dc49a5ae214ded1cc1" => :arm64_big_sur
    sha256 "7e34bd216191b40a86075d825c98c929d4f61842be989b605caba169ac68c999" => :catalina
    sha256 "a7de6d5c400b83a6f5e18423d396321aa45fb1a12dd1577df04389a7379e743a" => :mojave
    sha256 "e9b9b29b0f54e3349be1fad6f281d7ed0b972deaab07a0febe2ab75a73028ea5" => :high_sierra
  end

  def install
    system "./configure", "--disable-debug",
                          "--disable-dependency-tracking",
                          "--disable-silent-rules",
                          "--prefix=#{prefix}"
    system "make", "install"
    pkgshare.install "config.h", "test_ntlm.c", "test.txt", "gl/byteswap.h", "gl/md4.c", "gl/md4.h"
  end

  test do
    cp pkgshare.children, testpath
    system ENV.cc, "test_ntlm.c", "md4.c", "-I#{testpath}", "-L#{lib}", "-lntlm",
                   "-DNTLM_SRCDIR=\"#{testpath}\"", "-o", "test_ntlm"
    system "./test_ntlm"
  end
end
