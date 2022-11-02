class VorbisTools < Formula
  desc "Ogg Vorbis CODEC tools"
  homepage "https://github.com/xiph/vorbis-tools"
  url "https://downloads.xiph.org/releases/vorbis/vorbis-tools-1.4.0.tar.gz"
  sha256 "a389395baa43f8e5a796c99daf62397e435a7e73531c9f44d9084055a05d22bc"
  revision 2

  livecheck do
    url "https://downloads.xiph.org/releases/vorbis/"
    regex(/href=.*?vorbis-tools[._-]v?(\d+(?:\.\d+)+)\.t/i)
  end

  bottle do
    cellar :any
    rebuild 1
    sha256 "671568dbd6ae11ccaafc0e0a3a4bd467898bfa6d700b2e21db8457f900c44778" => :big_sur
    sha256 "99d9a734583553c8d008e5f068c1d95413dc52444b24a93e2ef1dd85b9b02614" => :arm64_big_sur
    sha256 "71a81bbeec2d79ddd7f39858cf66a450fac9d542824c30a064298229d6637594" => :catalina
    sha256 "c3e402519ad170a0a37d80d394d8afbe905985784f8ea5d93fcc84a4486a9977" => :mojave
    sha256 "e929c31331ffcb58d21cb086184ed747185dd8d0f4b7ee1b98134cabe44490bc" => :high_sierra
  end

  depends_on "pkg-config" => :build
  depends_on "flac"
  depends_on "libao"
  depends_on "libogg"
  depends_on "libvorbis"

  uses_from_macos "curl"

  def install
    args = %W[
      --disable-debug
      --disable-dependency-tracking
      --disable-nls
      --prefix=#{prefix}
    ]

    system "./configure", *args
    system "make", "install"
  end

  test do
    system bin/"oggenc", test_fixtures("test.wav"), "-o", "test.ogg"
    assert_predicate testpath/"test.ogg", :exist?
    output = shell_output("#{bin}/ogginfo test.ogg")
    assert_match "20.625000 kb/s", output
  end
end
