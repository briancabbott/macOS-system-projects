class Libgdata < Formula
  desc "GLib-based library for accessing online service APIs"
  homepage "https://wiki.gnome.org/Projects/libgdata"
  url "https://download.gnome.org/sources/libgdata/0.16/libgdata-0.16.1.tar.xz"
  sha256 "8740e071ecb2ae0d2a4b9f180d2ae5fdf9dc4c41e7ff9dc7e057f62442800827"
  revision 3

  livecheck do
    url :stable
  end

  bottle do
    sha256 "acf37716c065ba69fc22c35236bfc7ebdb7b01623e20ce82ec03306c2684b925" => :big_sur
    sha256 "e31807a39d967829c44e6fb94a2d3efba07e3e6fc8ae80747e18b435f46ed15f" => :arm64_big_sur
    sha256 "c93f83c348b673c9768be22ae9e1119d5eb86ff94bd28e95976c2dca47f5defe" => :catalina
    sha256 "e84e22686408f68d77b239d0cdc476f33e677f8aa66405ba4506513e31eafe2c" => :mojave
    sha256 "0320d28747a36cf8451eff40a16bc25c9735e287888177c2c1f1ec93a835cf56" => :high_sierra
  end

  depends_on "gobject-introspection" => :build
  depends_on "intltool" => :build
  depends_on "pkg-config" => :build
  depends_on "json-glib"
  depends_on "liboauth"
  depends_on "libsoup"

  # submitted upstream as https://bugzilla.gnome.org/show_bug.cgi?id=754821
  patch :DATA

  def install
    system "./configure", "--disable-dependency-tracking",
                          "--disable-silent-rules",
                          "--prefix=#{prefix}",
                          "--disable-gnome",
                          "--disable-tests"
    system "make", "install"
  end

  test do
    (testpath/"test.c").write <<~EOS
      #include <gdata/gdata.h>

      int main(int argc, char *argv[]) {
        GType type = gdata_comment_get_type();
        return 0;
      }
    EOS
    ENV.libxml2
    gettext = Formula["gettext"]
    glib = Formula["glib"]
    json_glib = Formula["json-glib"]
    liboauth = Formula["liboauth"]
    libsoup = Formula["libsoup"]
    flags = %W[
      -I#{gettext.opt_include}
      -I#{glib.opt_include}/glib-2.0
      -I#{glib.opt_lib}/glib-2.0/include
      -I#{include}/libgdata
      -I#{json_glib.opt_include}/json-glib-1.0
      -I#{liboauth.opt_include}
      -I#{libsoup.opt_include}/libsoup-2.4
      -I#{MacOS.sdk_path}/usr/include/libxml2
      -D_REENTRANT
      -L#{gettext.opt_lib}
      -L#{glib.opt_lib}
      -L#{json_glib.opt_lib}
      -L#{libsoup.opt_lib}
      -L#{lib}
      -lgdata
      -lgio-2.0
      -lglib-2.0
      -lgobject-2.0
      -lintl
      -ljson-glib-1.0
      -lsoup-2.4
      -lxml2
    ]
    system ENV.cc, "test.c", "-o", "test", *flags
    system "./test"
  end
end

__END__
diff --git a/gdata/gdata.symbols b/gdata/gdata.symbols
index bba24ec..c80a642 100644
--- a/gdata/gdata.symbols
+++ b/gdata/gdata.symbols
@@ -966,9 +966,6 @@ gdata_documents_entry_get_quota_used
 gdata_documents_service_copy_document
 gdata_documents_service_copy_document_async
 gdata_documents_service_copy_document_finish
-gdata_goa_authorizer_get_type
-gdata_goa_authorizer_new
-gdata_goa_authorizer_get_goa_object
 gdata_documents_document_get_thumbnail_uri
 gdata_tasks_task_get_type
 gdata_tasks_task_new
@@ -1089,8 +1086,6 @@ gdata_freebase_topic_value_is_image
 gdata_freebase_topic_result_get_type
 gdata_freebase_topic_result_new
 gdata_freebase_topic_result_dup_object
-gdata_freebase_result_error_get_type
-gdata_freebase_result_error_quark
 gdata_freebase_result_get_type
 gdata_freebase_result_new
 gdata_freebase_result_dup_variant
