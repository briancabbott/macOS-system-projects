class Libevent < Formula
  desc "Asynchronous event library"
  homepage "https://libevent.org/"
  url "https://github.com/libevent/libevent/archive/release-2.1.12-stable.tar.gz"
  sha256 "7180a979aaa7000e1264da484f712d403fcf7679b1e9212c4e3d09f5c93efc24"
  license "BSD-3-Clause"

  livecheck do
    url :homepage
    regex(/libevent[._-]v?(\d+(?:\.\d+)+)-stable/i)
  end

  bottle do
    cellar :any
    sha256 "45758b448d82b82b6bea52bc9a72593ef22f04ebdaa4b4230cadf12768252e22" => :big_sur
    sha256 "53ca41440aee8d770530b0de6e655d570028afe0a99ed0e4e16f83af29e06ecb" => :arm64_big_sur
    sha256 "b5f5e7607d76b9b41ecac6df72ab5797079a9367055bb305514917595e63a323" => :catalina
    sha256 "c52ad284f2bbdd512cd0ddd9fe19dd1b7092f7b967f593a7784098f5a1cdd254" => :mojave
    sha256 "bad1dc570cc96e2ed5654159d3dd382d94fbbda00ca26e6f5a5ddd7ce6cb6ed9" => :high_sierra
  end

  depends_on "autoconf" => :build
  depends_on "automake" => :build
  depends_on "libtool" => :build
  depends_on "pkg-config" => :build
  depends_on "openssl@1.1"

  def install
    system "./autogen.sh"
    system "./configure", "--disable-dependency-tracking",
                          "--disable-debug-mode",
                          "--prefix=#{prefix}"
    system "make"
    system "make", "install"
  end

  test do
    (testpath/"test.c").write <<~EOS
      #include <event2/event.h>

      int main()
      {
        struct event_base *base;
        base = event_base_new();
        event_base_free(base);
        return 0;
      }
    EOS
    system ENV.cc, "test.c", "-L#{lib}", "-levent", "-o", "test"
    system "./test"
  end
end
