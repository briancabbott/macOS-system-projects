class Proj < Formula
  desc "Cartographic Projections Library"
  homepage "https://proj.org/"
  url "https://github.com/OSGeo/PROJ/releases/download/7.2.1/proj-7.2.1.tar.gz"
  sha256 "b384f42e5fb9c6d01fe5fa4d31da2e91329668863a684f97be5d4760dbbf0a14"
  license "MIT"

  bottle do
    sha256 "dc7c6c5685716e0aa6d2bc58a118ff9767ee90f08a7ab7cf334e41acabbdd5ce" => :big_sur
    sha256 "943adb4dfac31337c9bbbaf30ae8cf4812455e291abee79a4c22b60aaca8f312" => :arm64_big_sur
    sha256 "0e9876b6205673f8f1536dbde4550e303d802c339c68601400575ac7fae4d0de" => :catalina
    sha256 "998ab40cb3bc023d4d4abd6a2efdcc7e91b1f6018a51bc9db6715ab864c1eb39" => :mojave
  end

  head do
    url "https://github.com/OSGeo/proj.git"
    depends_on "autoconf" => :build
    depends_on "automake" => :build
    depends_on "libtool" => :build
  end

  depends_on "pkg-config" => :build
  depends_on "libtiff"

  uses_from_macos "curl"
  uses_from_macos "sqlite"

  conflicts_with "blast", because: "both install a `libproj.a` library"

  skip_clean :la

  # The datum grid files are required to support datum shifting
  resource "datumgrid" do
    url "https://download.osgeo.org/proj/proj-datumgrid-1.8.zip"
    sha256 "b9838ae7e5f27ee732fb0bfed618f85b36e8bb56d7afb287d506338e9f33861e"
  end

  def install
    (buildpath/"nad").install resource("datumgrid")
    system "./autogen.sh" if build.head?
    system "./configure", "--disable-dependency-tracking",
                          "--prefix=#{prefix}"
    system "make", "install"
  end

  test do
    (testpath/"test").write <<~EOS
      45d15n 71d07w Boston, United States
      40d40n 73d58w New York, United States
      48d51n 2d20e Paris, France
      51d30n 7'w London, England
    EOS
    match = <<~EOS
      -4887590.49\t7317961.48 Boston, United States
      -5542524.55\t6982689.05 New York, United States
      171224.94\t5415352.81 Paris, France
      -8101.66\t5707500.23 London, England
    EOS

    output = shell_output("#{bin}/proj +proj=poly +ellps=clrk66 -r #{testpath}/test")
    assert_equal match, output
  end
end
