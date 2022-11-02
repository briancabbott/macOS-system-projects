class Openclonk < Formula
  desc "Multiplayer action game"
  homepage "https://www.openclonk.org/"
  url "https://www.openclonk.org/builds/release/7.0/openclonk-7.0-src.tar.bz2"
  sha256 "bc1a231d72774a7aa8819e54e1f79be27a21b579fb057609398f2aa5700b0732"
  license "ISC"
  revision 3
  head "https://github.com/openclonk/openclonk.git"

  bottle do
    cellar :any
    sha256 "1f4cca43144a36b7d6eeb24d9d3cefc84b591fb20abc503ecca7e73fc26b07ca" => :big_sur
    sha256 "ebd7f7efa0efc4c70b14071e98a5f2d314c16e5b6f28fe11257738619f0c813b" => :arm64_big_sur
    sha256 "95f44dd3686157a5185f1452f46515160347cef55237aac391edfabbbeb0c5de" => :catalina
    sha256 "688963d2df4cd964a51bed317cf656137d5e8d668b457a7cef89e8302ac02f49" => :mojave
    sha256 "87779de2d3cfa0dc1880fa45226e3f434ecca4409565db5e8bf278c225487da1" => :high_sierra
  end

  depends_on "cmake" => :build
  depends_on "boost"
  depends_on "freealut"
  depends_on "freetype"
  depends_on "glew"
  depends_on "jpeg"
  depends_on "libogg"
  depends_on "libpng"
  depends_on "libvorbis"

  def install
    ENV.cxx11
    system "cmake", ".", *std_cmake_args
    system "make"
    system "make", "install"
    bin.write_exec_script "#{prefix}/openclonk.app/Contents/MacOS/openclonk"
    bin.install Dir[prefix/"c4*"]
  end

  test do
    system bin/"c4group"
  end
end
