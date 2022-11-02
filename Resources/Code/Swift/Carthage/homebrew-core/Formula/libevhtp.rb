class Libevhtp < Formula
  desc "Create extremely-fast and secure embedded HTTP servers with ease"
  homepage "https://github.com/criticalstack/libevhtp/"
  url "https://github.com/criticalstack/libevhtp/archive/1.2.18.tar.gz"
  sha256 "316ede0d672be3ae6fe489d4ac1c8c53a1db7d4fe05edaff3c7c853933e02795"
  license "BSD-3-Clause"
  revision 3

  bottle do
    cellar :any
    sha256 "12b7dadd090f1b53ac313baf685f7bb73640aa6dc8bb34566cddf8ebdaf438f6" => :arm64_big_sur
    sha256 "507466763ef1710ef11b82d02a5229d1445ba6393a553d75926b8fe5d727d871" => :catalina
    sha256 "bfd6cffbcad95d0db38d4b699af24dd3aab1a82b0bdfc7ea7136b212cecab37c" => :mojave
    sha256 "72be53d01a0ab668255e9ab605c4d7b6c16e4ca1a3f68b026c3c9ae1fe77af50" => :high_sierra
  end

  depends_on "cmake" => :build
  depends_on "doxygen" => :build
  depends_on "libevent"
  depends_on "openssl@1.1"

  def install
    system "cmake", "-DEVHTP_BUILD_SHARED=ON",
                    "-DBUILD_SHARED_LIBS=ON",
                    "-DEVHTP_DISABLE_REGEX=ON",
                    ".", *std_cmake_args
    system "make", "install"
    mkdir_p "./html/docs/"
    system "doxygen", "Doxyfile"
    man3.install Dir["html/docs/man/man3/*.3"]
    doc.install Dir["html/docs/html/*"]
  end

  test do
    (testpath/"test.c").write <<~EOS
      #include <evhtp.h>

      int main() {
        struct event_base *base;
        struct evhtp *htp;
        base = event_base_new();
        htp = evhtp_new(base, NULL);
        evhtp_free(htp);
        event_base_free(base);
        return 0;
      }
    EOS

    system ENV.cc, "test.c",
                   "-I#{include}",
                   "-I#{Formula["openssl@1.1"].opt_include}",
                   "-I#{Formula["libevent"].opt_include}",
                   "-L#{Formula["openssl@1.1"].opt_lib}",
                   "-L#{Formula["libevent"].opt_lib}",
                   "-L#{lib}",
                   "-levhtp",
                   "-levent",
                   "-levent_openssl",
                   "-lssl",
                   "-lcrypto",
                   "-o", "test"
    system "./test"
  end
end
