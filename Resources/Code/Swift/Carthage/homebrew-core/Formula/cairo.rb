class Cairo < Formula
  desc "Vector graphics library with cross-device output support"
  homepage "https://cairographics.org/"
  url "https://cairographics.org/releases/cairo-1.16.0.tar.xz"
  sha256 "5e7b29b3f113ef870d1e3ecf8adf21f923396401604bda16d44be45e66052331"
  license any_of: ["LGPL-2.1-only", "MPL-1.1"]
  revision 4

  livecheck do
    url "https://cairographics.org/releases/?C=M&O=D"
    regex(%r{href=(?:["']?|.*?/)cairo[._-]v?(\d+\.\d*[02468](?:\.\d+)*)\.t}i)
  end

  bottle do
    sha256 "45f0b6aa6d76fa7806e1eeb066d6737033da3de74ac247a27735ff3a29b1b62b" => :big_sur
    sha256 "eb04d54ac340a4954a178e99d3ea064913d3fe89184b1edd479c2a96260bb989" => :arm64_big_sur
    sha256 "3d772a45e12f548338893e11cff0cd5c6a0a929bc214de8aa8cb6995c359bae9" => :catalina
    sha256 "9ab59fee2cf7e7c331b95a9d5f026dbfdc03b6fa761304f729cdf87921c786bf" => :mojave
  end

  head do
    url "https://gitlab.freedesktop.org/cairo/cairo.git"
    depends_on "autoconf" => :build
    depends_on "automake" => :build
    depends_on "libtool" => :build
  end

  depends_on "pkg-config" => :build
  depends_on "fontconfig"
  depends_on "freetype"
  depends_on "glib"
  depends_on "libpng"
  depends_on "lzo"
  depends_on "pixman"

  uses_from_macos "zlib"

  on_linux do
    depends_on "libx11"
    depends_on "libxcb"
    depends_on "libxext"
    depends_on "libxrender"
  end

  # Avoid segfaults on Big Sur. Remove at version bump.
  # https://gitlab.freedesktop.org/cairo/cairo/-/issues/420
  patch do
    url "https://gitlab.freedesktop.org/cairo/cairo/-/commit/e22d7212acb454daccc088619ee147af03883974.patch"
    sha256 "363a6018efc52721e2eace8df3aa319c93f3ad765ef7e3ea04e2ddd4ee94d0e1"
  end

  def install
    args = %W[
      --disable-dependency-tracking
      --prefix=#{prefix}
      --enable-gobject
      --enable-svg
      --enable-tee
      --disable-valgrind
    ]
    on_macos do
      args += %w[
        --enable-quartz-image
        --disable-xcb
        --disable-xlib
        --disable-xlib-xrender
      ]
    end
    on_linux do
      args += %w[
        --enable-xcb
        --enable-xlib
        --enable-xlib-xrender
      ]
    end

    if build.head?
      ENV["NOCONFIGURE"] = "1"
      system "./autogen.sh"
    end

    system "./configure", *args
    system "make", "install"
  end

  test do
    (testpath/"test.c").write <<~EOS
      #include <cairo.h>

      int main(int argc, char *argv[]) {

        cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 600, 400);
        cairo_t *context = cairo_create(surface);

        return 0;
      }
    EOS
    fontconfig = Formula["fontconfig"]
    freetype = Formula["freetype"]
    gettext = Formula["gettext"]
    glib = Formula["glib"]
    libpng = Formula["libpng"]
    pixman = Formula["pixman"]
    flags = %W[
      -I#{fontconfig.opt_include}
      -I#{freetype.opt_include}/freetype2
      -I#{gettext.opt_include}
      -I#{glib.opt_include}/glib-2.0
      -I#{glib.opt_lib}/glib-2.0/include
      -I#{include}/cairo
      -I#{libpng.opt_include}/libpng16
      -I#{pixman.opt_include}/pixman-1
      -L#{lib}
      -lcairo
    ]
    system ENV.cc, "test.c", "-o", "test", *flags
    system "./test"
  end
end
