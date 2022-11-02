class Theora < Formula
  desc "Open video compression format"
  homepage "https://www.theora.org/"
  url "https://downloads.xiph.org/releases/theora/libtheora-1.1.1.tar.bz2"
  sha256 "b6ae1ee2fa3d42ac489287d3ec34c5885730b1296f0801ae577a35193d3affbc"

  livecheck do
    url "https://www.theora.org/downloads/"
    regex(/href=.*?libtheora[._-]v?(\d+(?:\.\d+)+)\.t/i)
  end

  bottle do
    cellar :any
    rebuild 4
    sha256 "7ba57255df2c2f4563767031c6b9ead9a93883217644e9e64b5523e26d7b1088" => :big_sur
    sha256 "1e5271cb88e3dad8eb5f06460e7f3f2cec0129679c7f7fb4f84db1d09664b827" => :arm64_big_sur
    sha256 "1fcbd50039f580bd85554af2d831c28f83613b5d26969f577f7fe87b3c55db67" => :catalina
    sha256 "6fdb09d75fc6e64b266a185e711c2964e803d8f10c0d40ccb8d572c536c24d3a" => :mojave
  end

  head do
    url "https://gitlab.xiph.org/xiph/theora.git"

    depends_on "autoconf" => :build
    depends_on "automake" => :build
  end

  depends_on "libtool" => :build
  depends_on "pkg-config" => :build
  depends_on "libogg"
  depends_on "libvorbis"

  def install
    cp Dir["#{Formula["libtool"].opt_share}/libtool/*/config.{guess,sub}"], buildpath
    system "./autogen.sh" if build.head?

    args = %W[
      --disable-dependency-tracking
      --prefix=#{prefix}
      --disable-oggtest
      --disable-vorbistest
      --disable-examples
    ]

    args << "--disable-asm" if build.head?

    system "./configure", *args
    system "make", "install"
  end

  test do
    (testpath/"test.c").write <<~EOS
      #include <theora/theora.h>

      int main()
      {
          theora_info inf;
          theora_info_init(&inf);
          theora_info_clear(&inf);
          return 0;
      }
    EOS
    system ENV.cc, "test.c", "-L#{lib}", "-ltheora", "-o", "test"
    system "./test"
  end
end
