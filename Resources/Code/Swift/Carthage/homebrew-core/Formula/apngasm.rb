class Apngasm < Formula
  desc "Next generation of apngasm, the APNG assembler"
  homepage "https://github.com/apngasm/apngasm"
  url "https://github.com/apngasm/apngasm/archive/3.1.6.tar.gz"
  sha256 "0068e31cd878e07f3dffa4c6afba6242a753dac83b3799470149d2e816c1a2a7"
  license "Zlib"
  head "https://github.com/apngasm/apngasm.git"

  bottle do
    cellar :any
    rebuild 1
    sha256 "9f0a2fd1ce759c8d60e9f31deb74a33ae9714f7391255577fd33eb0d534b3cae" => :big_sur
    sha256 "89d28dc15d602243f648d782bf91931a2000feb1941fb452ee31bb13b8d50204" => :arm64_big_sur
    sha256 "48dfe150da3c6f5992d771f4581d39a6a4a9750ae4835d959f6bbc23c03923da" => :catalina
    sha256 "9385f652fd7fae83e373a823d7cd7b090213f3e6e17d1eee415022a4eb454cf4" => :mojave
    sha256 "df11f91ea8d27997410b38d42e435b58c34d54dd2bb58a714745ffcfaaacdda2" => :high_sierra
    sha256 "87cb9f81d1ec12b561e8750d259e27d1d97daa654742fcce032863e0185baf0f" => :sierra
    sha256 "073f74cacea8907e430113f4aae80d248887fc1d18de36f64889e683c08e3441" => :el_capitan
    sha256 "5fb1bd67761e2717c78c3842c7effd8835acb5f6193a05516df7cde7ff7051e8" => :yosemite
    sha256 "124cee4bf9746a9e60882cf6fd5b2430265fc661af5dbe9bab2f85e83e985cfa" => :mavericks
  end

  depends_on "cmake" => :build
  depends_on "boost"
  depends_on "libpng"
  depends_on "lzlib"

  def install
    inreplace "cli/CMakeLists.txt", "${CMAKE_INSTALL_PREFIX}/man/man1",
                                    "${CMAKE_INSTALL_PREFIX}/share/man/man1"
    system "cmake", ".", *std_cmake_args
    system "make", "install"
    (pkgshare/"test").install "test/samples"
  end

  test do
    system bin/"apngasm", "#{pkgshare}/test/samples/clock*.png"
  end
end
