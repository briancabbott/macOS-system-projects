class Mapcrafter < Formula
  desc "Minecraft map renderer"
  homepage "https://mapcrafter.org"
  url "https://github.com/mapcrafter/mapcrafter/archive/v.2.4.tar.gz"
  sha256 "f3b698d34c02c2da0c4d2b7f4e251bcba058d0d1e4479c0418eeba264d1c8dae"
  license "GPL-3.0"
  revision 3

  bottle do
    cellar :any
    sha256 "6b8ce30b655e743288ca868c0bdf231f45c6c7b094eb62797d3fa87c5b98e118" => :big_sur
    sha256 "b6a779b0bcd0f98e5b07cb6f2c9474b8974da9cba0f82e55311a8b253b74cd56" => :arm64_big_sur
    sha256 "986224321f7be4d777cb1ac012a237535c2cbb44586e86327cf84589fe85c327" => :catalina
    sha256 "ef50257242f50111c034ddc97be5d592b8a91d255053a2bb50b6cb9ea791e930" => :mojave
    sha256 "f3ce96014ce5e35f2a40034bd0498a583d4c92fe27ecc5ed3039733c1b049757" => :high_sierra
    sha256 "f0e35d940f533e1a4a8a3575afafe567523c89c72e81dc7276679c39b173800b" => :sierra
    sha256 "5b10b03e8125110487845f76b36dd5fea958e0d98b8f7ef14e72956f1c98b6f2" => :el_capitan
  end

  depends_on "cmake" => :build
  depends_on "boost"
  depends_on "jpeg-turbo"
  depends_on "libpng"

  def install
    ENV.cxx11

    args = std_cmake_args
    args << "-DJPEG_INCLUDE_DIR=#{Formula["jpeg-turbo"].opt_include}"
    args << "-DJPEG_LIBRARY=#{Formula["jpeg-turbo"].opt_lib}/libjpeg.dylib"

    system "cmake", ".", *args
    system "make", "install"
  end

  test do
    assert_match(/Mapcrafter/,
      shell_output("#{bin}/mapcrafter --version"))
  end
end
