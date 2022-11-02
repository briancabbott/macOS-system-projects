class Libffi < Formula
  desc "Portable Foreign Function Interface library"
  homepage "https://sourceware.org/libffi/"
  url "https://sourceware.org/pub/libffi/libffi-3.3.tar.gz"
  mirror "https://deb.debian.org/debian/pool/main/libf/libffi/libffi_3.3.orig.tar.gz"
  mirror "https://github.com/libffi/libffi/releases/download/v3.3/libffi-3.3.tar.gz"
  sha256 "72fba7922703ddfa7a028d513ac15a85c8d54c8d67f55fa5a4802885dc652056"
  license "MIT"
  revision 2

  bottle do
    cellar :any
    sha256 "b554c360440795f08f6afa353f467e152d82a80195ccca3f6e235d84366fea18" => :big_sur
    sha256 "101f73c4097df830a5f5ab4ad77da81c8dd1ce9c82e38676f7302aa09c3c236c" => :arm64_big_sur
    sha256 "1e976844c53c2a2462da41f0b6091e97dc82ecee6d2cf3063f818d44d8616cd7" => :catalina
    sha256 "3edbb019a2b682f31991ee1e520caf773254060b4cbaa78639c2f226b543a07c" => :mojave
  end

  head do
    url "https://github.com/atgreen/libffi.git"
    depends_on "autoconf" => :build
    depends_on "automake" => :build
    depends_on "libtool" => :build
  end

  keg_only :provided_by_macos

  on_macos do
    if Hardware::CPU.arm?
      # Improved aarch64-apple-darwin support. See https://github.com/libffi/libffi/pull/565
      patch do
        url "https://raw.githubusercontent.com/Homebrew/formula-patches/a4a91e61/libffi/libffi-3.3-arm64.patch"
        sha256 "ee084f76f69df29ed0fa1bc8957052cadc3bbd8cd11ce13b81ea80323f9cb4a3"
      end
    end
  end

  def install
    args = std_configure_args

    on_macos do
      # This can be removed in the future when libffi properly detects the CPU on ARM.
      # https://github.com/libffi/libffi/issues/571#issuecomment-655223391
      args << "--build=aarch64-apple-darwin#{OS.kernel_version}" if Hardware::CPU.arm?
    end

    system "./autogen.sh" if build.head?
    system "./configure", *args
    system "make", "install"
  end

  test do
    (testpath/"closure.c").write <<~EOS
      #include <stdio.h>
      #include <ffi.h>

      /* Acts like puts with the file given at time of enclosure. */
      void puts_binding(ffi_cif *cif, unsigned int *ret, void* args[],
                        FILE *stream)
      {
        *ret = fputs(*(char **)args[0], stream);
      }

      int main()
      {
        ffi_cif cif;
        ffi_type *args[1];
        ffi_closure *closure;

        int (*bound_puts)(char *);
        int rc;

        /* Allocate closure and bound_puts */
        closure = ffi_closure_alloc(sizeof(ffi_closure), &bound_puts);

        if (closure)
          {
            /* Initialize the argument info vectors */
            args[0] = &ffi_type_pointer;

            /* Initialize the cif */
            if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 1,
                             &ffi_type_uint, args) == FFI_OK)
              {
                /* Initialize the closure, setting stream to stdout */
                if (ffi_prep_closure_loc(closure, &cif, puts_binding,
                                         stdout, bound_puts) == FFI_OK)
                  {
                    rc = bound_puts("Hello World!");
                    /* rc now holds the result of the call to fputs */
                  }
              }
          }

        /* Deallocate both closure, and bound_puts */
        ffi_closure_free(closure);

        return 0;
      }
    EOS

    flags = ["-L#{lib}", "-lffi", "-I#{include}"]
    system ENV.cc, "-o", "closure", "closure.c", *(flags + ENV.cflags.to_s.split)
    system "./closure"
  end
end
