class Uchardet < Formula
  desc "Encoding detector library"
  homepage "https://www.freedesktop.org/wiki/Software/uchardet/"
  url "https://www.freedesktop.org/software/uchardet/releases/uchardet-0.0.7.tar.xz"
  sha256 "3fc79408ae1d84b406922fa9319ce005631c95ca0f34b205fad867e8b30e45b1"
  head "https://gitlab.freedesktop.org/uchardet/uchardet.git"

  livecheck do
    url :stable
  end

  bottle do
    cellar :any
    sha256 "f5125cd193034470e278536d8b5d2498e99ddbd5c2853e214fe24fe845cc56d7" => :big_sur
    sha256 "33989aa08f1ffc6f71f536c2a3cb28aabcc774e90da2aa40db110048f4b38238" => :arm64_big_sur
    sha256 "34bd4791834a762da961de136a3cad253800a98deea2d1dededf8efb465f215d" => :catalina
    sha256 "a254139ee777de77d5907dfeb47d4306f2e5e5f0b3775edfedcbf1fcf217fdbe" => :mojave
    sha256 "a89237dd88e4190826ce6fb4a8a625711636db7d6c4ba10d5ddd56d37347e868" => :high_sierra
  end

  depends_on "cmake" => :build

  def install
    args = std_cmake_args
    args << "-DCMAKE_INSTALL_NAME_DIR=#{lib}"
    system "cmake", ".", *args
    system "make", "install"
  end

  test do
    assert_equal "ASCII", pipe_output("#{bin}/uchardet", "Homebrew").chomp
  end
end
