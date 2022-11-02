class Libsigsegv < Formula
  desc "Library for handling page faults in user mode"
  homepage "https://www.gnu.org/software/libsigsegv/"
  url "https://ftp.gnu.org/gnu/libsigsegv/libsigsegv-2.12.tar.gz"
  mirror "https://ftpmirror.gnu.org/libsigsegv/libsigsegv-2.12.tar.gz"
  sha256 "3ae1af359eebaa4ffc5896a1aee3568c052c99879316a1ab57f8fe1789c390b6"
  license "GPL-2.0-or-later"

  livecheck do
    url :stable
  end

  bottle do
    cellar :any
    rebuild 1
    sha256 "8f4dde47fdc37a7d8bfe2e5fb5a6935df61933ac0c261c7e84a779ea9d1571f9" => :big_sur
    sha256 "3f08091c87658aaf556a9309ce98146faee3d9be07e72380fdab78449111195c" => :catalina
    sha256 "f883382e7eb115a7ea8f660487b40f186d2d422f16d0bceaa58ceca19f7279f9" => :mojave
  end

  head do
    url "https://git.savannah.gnu.org/git/libsigsegv.git"

    depends_on "autoconf" => :build
    depends_on "automake" => :build
  end

  def install
    system "./gitsub.sh", "pull" if build.head?
    system "./autogen.sh" if build.head?
    system "./configure", "--disable-dependency-tracking",
                          "--prefix=#{prefix}",
                          "--enable-shared"
    system "make"
    system "make", "check"
    system "make", "install"
  end

  test do
    # Sourced from tests/efault1.c in tarball.
    (testpath/"test.c").write <<~EOS
      #include "sigsegv.h"

      #include <errno.h>
      #include <fcntl.h>
      #include <stdio.h>
      #include <stdlib.h>
      #include <unistd.h>

      const char *null_pointer = NULL;
      static int
      handler (void *fault_address, int serious)
      {
        abort ();
      }

      int
      main ()
      {
        if (open (null_pointer, O_RDONLY) != -1 || errno != EFAULT)
          {
            fprintf (stderr, "EFAULT not detected alone");
            exit (1);
          }

        if (sigsegv_install_handler (&handler) < 0)
          exit (2);

        if (open (null_pointer, O_RDONLY) != -1 || errno != EFAULT)
          {
            fprintf (stderr, "EFAULT not detected with handler");
            exit (1);
          }

        printf ("Test passed");
        return 0;
      }
    EOS

    system ENV.cc, "test.c", "-L#{lib}", "-lsigsegv", "-o", "test"
    assert_match /Test passed/, shell_output("./test")
  end
end
