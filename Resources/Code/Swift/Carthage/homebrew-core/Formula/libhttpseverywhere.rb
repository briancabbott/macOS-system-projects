class Libhttpseverywhere < Formula
  desc "Bring HTTPSEverywhere to desktop apps"
  homepage "https://github.com/gnome/libhttpseverywhere"
  url "https://download.gnome.org/sources/libhttpseverywhere/0.8/libhttpseverywhere-0.8.3.tar.xz"
  sha256 "1c006f5633842a2b131c1cf644ab929556fc27968a60da55c00955bd4934b6ca"
  license "LGPL-3.0-or-later"
  revision 4

  livecheck do
    url :stable
  end

  bottle do
    cellar :any
    sha256 "459d83997d7d69966ddee1e7a94e8583b4de8570ee1a796273a64a3d7845b8cd" => :big_sur
    sha256 "006bf3748d65067509e5b2e6d506f3b0a9a52c5eaab54780850b70b7f82ff249" => :arm64_big_sur
    sha256 "c8cc1d294949af9676e54f9a32c4dbe782dfc5d103f92bbee68acd2ccb5ff728" => :catalina
    sha256 "2835c48e21e0a96730893f96319736e55d29d8b224fcc0915e319bcbc3b521c2" => :mojave
    sha256 "9c7c9397a0ebe56b82ffa6d8daeb9e645e94d14ed4fd25aedbe313c603e0b9b5" => :high_sierra
  end

  depends_on "gobject-introspection" => :build
  depends_on "meson" => :build
  depends_on "ninja" => :build
  depends_on "pkg-config" => :build
  depends_on "vala" => :build
  depends_on "glib"
  depends_on "json-glib"
  depends_on "libarchive"
  depends_on "libgee"
  depends_on "libsoup"

  # see https://gitlab.gnome.org/GNOME/libhttpseverywhere/issues/1
  # remove when next version is released
  patch do
    url "https://gitlab.gnome.org/GNOME/libhttpseverywhere/commit/6da08ef1ade9ea267cecf14dd5cb2c3e6e5e50cb.patch"
    sha256 "511c5aa10f466e879e04e794e09716de6bb18413bd23a72cffb323be5a982919"
  end

  def install
    mkdir "build" do
      system "meson", *std_meson_args, ".."
      system "ninja"
      system "ninja", "install"
    end
  end

  test do
    (testpath/"test.c").write <<~EOS
      #include <httpseverywhere.h>

      int main(int argc, char *argv[]) {
        GType type = https_everywhere_context_get_type();
        return 0;
      }
    EOS
    ENV.libxml2
    gettext = Formula["gettext"]
    glib = Formula["glib"]
    json_glib = Formula["json-glib"]
    libarchive = Formula["libarchive"]
    libgee = Formula["libgee"]
    libsoup = Formula["libsoup"]
    pcre = Formula["pcre"]
    flags = (ENV.cflags || "").split + (ENV.cppflags || "").split + (ENV.ldflags || "").split
    flags += %W[
      -I#{gettext.opt_include}
      -I#{glib.opt_include}/glib-2.0
      -I#{glib.opt_lib}/glib-2.0/include
      -I#{include}/httpseverywhere-0.8
      -I#{json_glib.opt_include}/json-glib-1.0
      -I#{libarchive.opt_include}
      -I#{libgee.opt_include}/gee-0.8
      -I#{libsoup.opt_include}/libsoup-2.4
      -I#{pcre.opt_include}
      -D_REENTRANT
      -L#{gettext.opt_lib}
      -L#{glib.opt_lib}
      -L#{json_glib.opt_lib}
      -L#{libarchive.opt_lib}
      -L#{libgee.opt_lib}
      -L#{libsoup.opt_lib}
      -L#{lib}
      -larchive
      -lgee-0.8
      -lgio-2.0
      -lglib-2.0
      -lgobject-2.0
      -lhttpseverywhere-0.8
      -lintl
      -ljson-glib-1.0
      -lsoup-2.4
      -lxml2
    ]
    system ENV.cc, "test.c", "-o", "test", *flags
    system "./test"
  end
end
