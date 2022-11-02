class Brainfuck < Formula
  desc "Interpreter for the brainfuck language"
  homepage "https://github.com/fabianishere/brainfuck"
  url "https://github.com/fabianishere/brainfuck/archive/2.7.1.tar.gz"
  sha256 "06534de715dbc614f08407000c2ec6d497770069a2d7c84defd421b137313d71"
  license "Apache-2.0"
  head "https://github.com/fabianishere/brainfuck.git"

  bottle do
    cellar :any
    sha256 "be92f674de1067730847cbfa767bc7ef13ce1507604e6261dcc4244a23a1f75e" => :big_sur
    sha256 "560663a6526b7ebc8f4eb49ffa77baaa5d52fd0c4567310ce89adb4e0175c7a5" => :arm64_big_sur
    sha256 "bf49cdb2f3515c537a9ced6a5697be843489b2bf37b3ea732527077e96347f04" => :catalina
    sha256 "a9e9509e4f0cd5345ffeac741c3a74d5575e17c99ea53822dd77fd4f98687b57" => :mojave
    sha256 "cf3c31fcf7c4cf099b348d01e619d1791aa3a255199de80afbc637e331947abf" => :high_sierra
    sha256 "354bb3372301325b49bfd4bd9b53084061af3bc3a3d6375e1c4635297c0dd008" => :sierra
    sha256 "f8289bed7e6455b63f05baf367069f60fe478f6c78f064c06ab1e571a181c3b7" => :el_capitan
  end

  depends_on "cmake" => :build

  def install
    system "cmake", ".", *std_cmake_args, "-DBUILD_SHARED_LIB=ON",
                         "-DBUILD_STATIC_LIB=ON", "-DINSTALL_EXAMPLES=ON"
    system "make", "install"
  end

  test do
    output = shell_output("#{bin}/brainfuck -e '++++++++[>++++++++<-]>+.+.+.'")
    assert_equal "ABC", output.chomp
  end
end
