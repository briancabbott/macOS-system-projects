class Taglib < Formula
  desc "Audio metadata library"
  homepage "https://taglib.github.io/"
  url "https://taglib.github.io/releases/taglib-1.11.1.tar.gz"
  sha256 "b6d1a5a610aae6ff39d93de5efd0fdc787aa9e9dc1e7026fa4c961b26563526b"
  license "LGPL-2.1"
  head "https://github.com/taglib/taglib.git"

  bottle do
    cellar :any
    sha256 "403004b0afd61b06a1e018a20d2e7760f04daa78d262996e317d47051a57ee11" => :big_sur
    sha256 "1345e85f67a1246d97fb16bc3c30db0a5e2ae469995261fbea632df757598975" => :arm64_big_sur
    sha256 "678392b9ac6fbc17a70433b5a98630ccbfa0b71eb1475402d826e4052086f246" => :catalina
    sha256 "98f103a3174694dd9ff58661cb83c08180049681ac1768b55b447dd99874150d" => :mojave
    sha256 "14e9be9fd1d5a86615d8b2b6ac51893eb6fab0eb6100f44547d297ccadc4497e" => :high_sierra
    sha256 "a0a374439cbf94a6fb57d791abf0bc6fb974eef1cf21f66c2731d1fc83d2428d" => :sierra
    sha256 "edaf79d2a2ec72ae32d9b46621697626a27299226a6b4d963431da8c37d3af52" => :el_capitan
    sha256 "bfda081fd34cb47bcdfd41e814612dbdf330166e30e69867cf43fcac60e5ed1a" => :yosemite
  end

  depends_on "cmake" => :build

  def install
    system "cmake", "-DWITH_MP4=ON", "-DWITH_ASF=ON", "-DBUILD_SHARED_LIBS=ON",
                    *std_cmake_args
    system "make", "install"
  end

  test do
    assert_match version.to_s, shell_output("#{bin}/taglib-config --version")
  end
end
