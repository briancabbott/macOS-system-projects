class SpiceGtk < Formula
  desc "GTK client/libraries for SPICE"
  homepage "https://www.spice-space.org"
  url "https://www.spice-space.org/download/gtk/spice-gtk-0.37.tar.bz2"
  sha256 "1f28b706472ad391cda79a93fd7b4c7a03e84b88fc46ddb35dddbe323c923bb7"
  revision 4

  livecheck do
    url "https://www.spice-space.org/download/gtk/"
    regex(/href=.*?spice-gtk[._-]v?(\d+(?:\.\d+)+)\.t/i)
  end

  bottle do
    sha256 "668a907575696c6fbbcc26cb5ebc831d1e4a80b90607e6cc44758c27923ec565" => :big_sur
    sha256 "0e79a73e33dd941c0011d5dab31fd330bd7fe7833d4a16f3d66ebf0fa431a2c3" => :catalina
    sha256 "429a96412033c4c47ce892cbac6a43b7e9ad8523438f6d0ad532d8c8d3ee53ce" => :mojave
    sha256 "32a55dcaa4902143f4fda24ca035ee3f1be41267d862e46bc3f7ba7a7181d026" => :high_sierra
  end

  depends_on "autoconf" => :build
  depends_on "autogen" => :build
  depends_on "automake" => :build
  depends_on "gobject-introspection" => :build
  depends_on "intltool" => :build
  depends_on "libtool" => :build
  depends_on "pkg-config" => :build
  depends_on "vala" => :build

  depends_on "atk"
  depends_on "cairo"
  depends_on "gdk-pixbuf"
  depends_on "gettext"
  depends_on "glib"
  depends_on "gst-libav"
  depends_on "gst-plugins-bad"
  depends_on "gst-plugins-base"
  depends_on "gst-plugins-good"
  depends_on "gst-plugins-ugly"
  depends_on "gstreamer"
  depends_on "gtk+3"
  depends_on "jpeg"
  depends_on "json-glib"
  depends_on "libusb"
  depends_on "lz4"
  depends_on "openssl@1.1"
  depends_on "opus"
  depends_on "pango"
  depends_on "pixman"
  depends_on "spice-protocol"
  depends_on "usbredir"

  # Upstream patch: https://gitlab.freedesktop.org/spice/spice-gtk/issues/88
  patch do
    url "https://gitlab.freedesktop.org/spice/spice-gtk/commit/3c9b37bfc7c88969dfe16b8bfd874745e0fceb8a.patch"
    sha256 "893878f2682d663cce38b9769f08e68fd7d60d650f4974728f380847d87dcbc1"
  end

  def install
    args = %W[
      --disable-dependency-tracking
      --disable-silent-rules
      --enable-introspection
      --enable-gstvideo
      --enable-gstaudio
      --enable-gstreamer=1.0
      --enable-vala
      --with-coroutine=gthread
      --with-gtk=3.0
      --with-lz4
      --prefix=#{prefix}
    ]
    system "autoreconf"
    system "./configure", *args
    system "make", "install"
  end

  test do
    (testpath/"test.cpp").write <<~EOS
      #include <spice-client.h>
      #include <spice-client-gtk.h>
      int main() {
        return spice_session_new() ? 0 : 1;
      }
    EOS
    system ENV.cc, "test.cpp",
                   "-I#{Formula["atk"].include}/atk-1.0",
                   "-I#{Formula["cairo"].include}/cairo",
                   "-I#{Formula["gdk-pixbuf"].include}/gdk-pixbuf-2.0",
                   "-I#{Formula["glib"].include}/glib-2.0",
                   "-I#{Formula["glib"].lib}/glib-2.0/include",
                   "-I#{Formula["gtk+3"].include}/gtk-3.0",
                   "-I#{Formula["harfbuzz"].opt_include}/harfbuzz",
                   "-I#{Formula["pango"].include}/pango-1.0",
                   "-I#{Formula["spice-protocol"].include}/spice-1",
                   "-I#{include}/spice-client-glib-2.0",
                   "-I#{include}/spice-client-gtk-3.0",
                   "-L#{lib}",
                   "-lspice-client-glib-2.0",
                   "-lspice-client-gtk-3.0",
                   "-o", "test"
    system "./test"
  end
end
