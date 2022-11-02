class Vstr < Formula
  desc "C string library"
  homepage "http://www.and.org/vstr/"
  url "http://www.and.org/vstr/1.0.15/vstr-1.0.15.tar.bz2"
  sha256 "d33bcdd48504ddd21c0d53e4c2ac187ff6f0190d04305e5fe32f685cee6db640"
  license "LGPL-2.1"

  bottle do
    cellar :any
    sha256 "cc1c69c834bde35ed9e0df8178e8e65d9ba5703fbf2cf896290aed6a7433c4b3" => :big_sur
    sha256 "adbf13e88473af357032472ac09af1230667c5010089089a3c223819ef74c7f6" => :catalina
    sha256 "8927c49aa4daba57ffab9a9ea332504346467cf22c137af3e1a16b859318a0f5" => :mojave
    sha256 "af6d9cc097c4eb9c1719496b2e29593763b5b17b279ef4c234d681cfe4174b37" => :high_sierra
    sha256 "07e2b05d9908a847c72950532d3ed12771c856365c8747c8c5917da9a5ea4413" => :sierra
    sha256 "d2d5b14e9ac589c782307e058e06815ad2408bbcf418ac721d3fac3be8b832a7" => :el_capitan
    sha256 "0d4176307ea18472c9da9a765bcb033e6256ae361d2e32b758b205a56dd7e38a" => :yosemite
    sha256 "5fc509c660fc38b3484a093ce0894ee2e9c3ea3cccc43222071c4d8139975d8d" => :mavericks
  end

  depends_on "pkg-config" => :build

  def install
    ENV.append "CFLAGS", "--std=gnu89" if ENV.compiler == :clang
    system "./configure", "--disable-dependency-tracking",
                          "--prefix=#{prefix}",
                          "--mandir=#{man}"
    system "make", "install"
  end

  test do
    (testpath/"test.c").write <<~EOS
      // based on http://www.and.org/vstr/examples/ex_hello_world.c
      #define VSTR_COMPILE_INCLUDE 1
      #include <vstr.h>
      #include <errno.h>
      #include <err.h>
      #include <unistd.h>

      int main(void) {
        Vstr_base *s1 = NULL;

        if (!vstr_init())
          err(EXIT_FAILURE, "init");

        if (!(s1 = vstr_dup_cstr_buf(NULL, "Hello Homebrew\\n")))
          err(EXIT_FAILURE, "Create string");

        while (s1->len)
          if (!vstr_sc_write_fd(s1, 1, s1->len, STDOUT_FILENO, NULL)) {
            if ((errno != EAGAIN) && (errno != EINTR))
              err(EXIT_FAILURE, "write");
          }

        vstr_free_base(s1);
        vstr_exit();
      }
    EOS

    system ENV.cc, "test.c", "-L#{lib}", "-lvstr", "-o", "test"
    system "./test"
  end
end
