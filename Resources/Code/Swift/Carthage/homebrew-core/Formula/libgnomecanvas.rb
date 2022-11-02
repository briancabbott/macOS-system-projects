class Libgnomecanvas < Formula
  desc "Highlevel, structured graphics library"
  homepage "https://developer.gnome.org/libgnomecanvas/2.30/"
  url "https://download.gnome.org/sources/libgnomecanvas/2.30/libgnomecanvas-2.30.3.tar.bz2"
  sha256 "859b78e08489fce4d5c15c676fec1cd79782f115f516e8ad8bed6abcb8dedd40"
  revision 5

  livecheck do
    url :stable
  end

  bottle do
    cellar :any
    sha256 "e2ae279ca7759e74bf93ed0577838d7e80fef134ad5f76c671263d023bca3dd1" => :big_sur
    sha256 "ed9d17d2b7100e9c5ef536c547119eb78e8658bc273f958e673d47383290c3d7" => :arm64_big_sur
    sha256 "816cd9bf11520fba1126073191c236f2355c45a137905ba978f16a506960fef0" => :catalina
    sha256 "bedab86245aa4185fc9c009496ec2d0fc0d1ea53074493db08afc81bdf424a60" => :mojave
  end

  depends_on "intltool" => :build
  depends_on "pkg-config" => :build
  depends_on "gettext"
  depends_on "gtk+"
  depends_on "libart"
  depends_on "libglade"

  def install
    system "./configure", "--disable-dependency-tracking",
                          "--disable-static",
                          "--prefix=#{prefix}",
                          "--enable-glade"
    system "make", "install"
  end

  test do
    (testpath/"test.c").write <<~EOS
      #include <libgnomecanvas/libgnomecanvas.h>

      int main(int argc, char *argv[]) {
        GnomeCanvasPoints *gcp = gnome_canvas_points_new(100);
        return 0;
      }
    EOS
    atk = Formula["atk"]
    cairo = Formula["cairo"]
    fontconfig = Formula["fontconfig"]
    freetype = Formula["freetype"]
    gdk_pixbuf = Formula["gdk-pixbuf"]
    gettext = Formula["gettext"]
    glib = Formula["glib"]
    gtkx = Formula["gtk+"]
    harfbuzz = Formula["harfbuzz"]
    libart = Formula["libart"]
    libpng = Formula["libpng"]
    pango = Formula["pango"]
    pixman = Formula["pixman"]
    flags = %W[
      -I#{atk.opt_include}/atk-1.0
      -I#{cairo.opt_include}/cairo
      -I#{fontconfig.opt_include}
      -I#{freetype.opt_include}/freetype2
      -I#{gdk_pixbuf.opt_include}/gdk-pixbuf-2.0
      -I#{gettext.opt_include}
      -I#{glib.opt_include}/glib-2.0
      -I#{glib.opt_lib}/glib-2.0/include
      -I#{gtkx.opt_include}/gail-1.0
      -I#{gtkx.opt_include}/gtk-2.0
      -I#{gtkx.opt_lib}/gtk-2.0/include
      -I#{harfbuzz.opt_include}/harfbuzz
      -I#{include}/libgnomecanvas-2.0
      -I#{libart.opt_include}/libart-2.0
      -I#{libpng.opt_include}/libpng16
      -I#{pango.opt_include}/pango-1.0
      -I#{pixman.opt_include}/pixman-1
      -D_REENTRANT
      -L#{atk.opt_lib}
      -L#{cairo.opt_lib}
      -L#{gdk_pixbuf.opt_lib}
      -L#{gettext.opt_lib}
      -L#{glib.opt_lib}
      -L#{gtkx.opt_lib}
      -L#{libart.opt_lib}
      -L#{lib}
      -L#{pango.opt_lib}
      -lart_lgpl_2
      -latk-1.0
      -lcairo
      -lgdk-quartz-2.0
      -lgdk_pixbuf-2.0
      -lgio-2.0
      -lglib-2.0
      -lgnomecanvas-2
      -lgobject-2.0
      -lgtk-quartz-2.0
      -lintl
      -lpango-1.0
      -lpangocairo-1.0
    ]
    system ENV.cc, "test.c", "-o", "test", *flags
    system "./test"
  end
end
