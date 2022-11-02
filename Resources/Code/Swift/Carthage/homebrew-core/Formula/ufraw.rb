class Ufraw < Formula
  desc "Unidentified Flying RAW: RAW image processing utility"
  homepage "https://ufraw.sourceforge.io"
  url "https://downloads.sourceforge.net/project/ufraw/ufraw/ufraw-0.22/ufraw-0.22.tar.gz"
  sha256 "f7abd28ce587db2a74b4c54149bd8a2523a7ddc09bedf4f923246ff0ae09a25e"
  revision 3

  livecheck do
    url :stable
  end

  bottle do
    rebuild 1
    sha256 "0abcda85255bcf73260764126f3e6213c439e68cb8be30e712319d83361a236c" => :big_sur
    sha256 "f58b3545d468e343cff5fa82581c8888f60557e6c7badfbf1f6094f1444ac601" => :arm64_big_sur
    sha256 "19a95667ecb2a9bab8a108e539ef229b945f727bca7e8651af80cca1d355a196" => :catalina
    sha256 "d880967d58bbbefb118148da4c959e38a3409a67504f21ae9b53560884da192f" => :mojave
    sha256 "e09fbf5a78f3b461637d21e13575330232de1c70dd3e63026ab0dcc5669905e3" => :high_sierra
  end

  depends_on "pkg-config" => :build
  depends_on "dcraw"
  depends_on "gettext"
  depends_on "glib"
  depends_on "jasper"
  depends_on "jpeg"
  depends_on "libpng"
  depends_on "libtiff"
  depends_on "little-cms2"

  # jpeg 9 compatibility
  patch do
    url "https://raw.githubusercontent.com/Homebrew/formula-patches/b8ed064e0d2567a4ced511755ba0a8cc5ecc75f7/ufraw/jpeg9.patch"
    sha256 "45de293a9b132eb675302ba8870f5b6216c51da8247cd096b24a5ab60ffbd7f9"
  end

  # Fix compilation with Xcode 9 and later,
  # see https://sourceforge.net/p/ufraw/bugs/419/
  patch do
    url "https://raw.githubusercontent.com/Homebrew/formula-patches/d5bf686c740d9ee0fdf0384ef8dfb293c5483dd2/ufraw/high_sierra.patch"
    sha256 "60c67978cc84b5a118855bcaa552d5c5c3772b407046f1b9db9b74076a938f6e"
  end

  def install
    system "./configure", "--disable-dependency-tracking",
                          "--prefix=#{prefix}",
                          "--without-gtk",
                          "--without-gimp"
    system "make", "install"
    (share/"pixmaps").rmtree
  end

  test do
    assert_match version.to_s, shell_output("#{bin}/ufraw-batch --version 2>&1")
  end
end
