class Libcddb < Formula
  desc "CDDB server access library"
  homepage "https://libcddb.sourceforge.io/"
  url "https://downloads.sourceforge.net/project/libcddb/libcddb/1.3.2/libcddb-1.3.2.tar.bz2"
  sha256 "35ce0ee1741ea38def304ddfe84a958901413aa829698357f0bee5bb8f0a223b"
  revision 4

  livecheck do
    url :stable
  end

  bottle do
    cellar :any
    rebuild 2
    sha256 "e19fbf67a440482346f40076ceae29a8b72590ef1376e6c5454d9f7814984e3b" => :big_sur
    sha256 "5c01ee6149ed61a23ad7d8a2c09250fedf3b605638552fe82057cf77b0ac61f1" => :arm64_big_sur
    sha256 "ca3cb9caeed526ef59a167293871d7b739c2ee6271571225dd1640f4af101140" => :catalina
    sha256 "534e9e7afc756a552c414b224d86ffa84c9966bbccf3a7d781a6b55a482e9bdf" => :mojave
  end

  depends_on "pkg-config" => :build
  depends_on "libcdio"

  def install
    system "./configure", "--disable-dependency-tracking",
                          "--prefix=#{prefix}"
    system "make", "install"
  end

  test do
    (testpath/"test.c").write <<~EOS
      #include <cddb/cddb.h>
      int main(void) {
        cddb_track_t *track = cddb_track_new();
        cddb_track_destroy(track);
      }
    EOS
    system ENV.cc, "test.c", "-L#{lib}", "-lcddb", "-o", "test"
    system "./test"
  end
end
