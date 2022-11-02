class Libraw < Formula
  desc "Library for reading RAW files from digital photo cameras"
  homepage "https://www.libraw.org/"
  url "https://www.libraw.org/data/LibRaw-0.20.2.tar.gz"
  sha256 "dc1b486c2003435733043e4e05273477326e51c3ea554c6864a4eafaff1004a6"
  license any_of: ["LGPL-2.1-only", "CDDL-1.0"]

  livecheck do
    url "https://www.libraw.org/download/"
    regex(/href=.*?LibRaw[._-]v?(\d+(?:\.\d+)*)\.t/i)
  end

  bottle do
    cellar :any
    sha256 "89a0ce1dc2548f25b9813e32b9a234b44c3dab57adaf98fa204ca35f75b2f2eb" => :big_sur
    sha256 "91215928c6f416f820d2d73c4be22175a00a7c9aa555b2dc01a3f649d888e20f" => :arm64_big_sur
    sha256 "465ba53999bd9b1297fed5283b8cf09ccc600fc1ca00ea4be58a6193a96910d4" => :catalina
    sha256 "2c62cfe8160a4cf45819255cc2a1bb77427827bfa7c4cf782a42f95ee9822112" => :mojave
    sha256 "dd4ab6f94e1dc725cb23ea1f4ee0267b632e01c7187c7a2b109053f457a17284" => :high_sierra
  end

  depends_on "autoconf" => :build
  depends_on "automake" => :build
  depends_on "libtool" => :build
  depends_on "pkg-config" => :build
  depends_on "jasper"
  depends_on "jpeg"
  depends_on "libomp"
  depends_on "little-cms2"

  resource "librawtestfile" do
    url "https://www.rawsamples.ch/raws/nikon/d1/RAW_NIKON_D1.NEF"
    sha256 "7886d8b0e1257897faa7404b98fe1086ee2d95606531b6285aed83a0939b768f"
  end

  def install
    system "autoreconf", "-fiv"
    system "./configure", "--prefix=#{prefix}",
                          "--disable-dependency-tracking",
                          "ac_cv_prog_c_openmp=-Xpreprocessor -fopenmp",
                          "ac_cv_prog_cxx_openmp=-Xpreprocessor -fopenmp",
                          "LDFLAGS=-lomp"
    system "make"
    system "make", "install"
    doc.install Dir["doc/*"]
    prefix.install "samples"
  end

  test do
    resource("librawtestfile").stage do
      filename = "RAW_NIKON_D1.NEF"
      system "#{bin}/raw-identify", "-u", filename
      system "#{bin}/simple_dcraw", "-v", "-T", filename
    end
  end
end
