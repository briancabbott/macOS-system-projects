class Viennacl < Formula
  desc "Linear algebra library for many-core architectures and multi-core CPUs"
  homepage "https://viennacl.sourceforge.io/"
  url "https://downloads.sourceforge.net/project/viennacl/1.7.x/ViennaCL-1.7.1.tar.gz"
  sha256 "a596b77972ad3d2bab9d4e63200b171cd0e709fb3f0ceabcaf3668c87d3a238b"
  head "https://github.com/viennacl/viennacl-dev.git"

  livecheck do
    url :stable
  end

  bottle do
    cellar :any_skip_relocation
    sha256 "696235e232844f8af5d062bc9197ab87fcacb012da49304b7cce059b145255cf" => :big_sur
    sha256 "a59335b82a9f92448236ec0278d156f2425995d48fddcef730b906ca63aea6f9" => :arm64_big_sur
    sha256 "6fa1cf4450123da7e4af2910f6a9c41e7005d5591e05d035c06adddff44f25e0" => :catalina
    sha256 "0d2ae6a32779520d35e8194948a0df499bc147743fd54f59fe3c69e833e84f1c" => :mojave
    sha256 "7be4bc5f161868a9646a575530acd83034e7af6e39439e262c499b219738e74e" => :high_sierra
    sha256 "809b0ff014ad6fdae2337ac8dd0cde29c72fe4cb8817a7e7417e9722b7572059" => :sierra
    sha256 "cb5cd96fd4c730518b6b0e150fd15386ad71576e444bfbbd5f055e844d4a5976" => :el_capitan
    sha256 "875f61b8270246247450c0beedc9710b52d07171717dd2f9de9a493f3b4027b6" => :yosemite
    sha256 "7256e29352bcf349fda479ef6913241249db48065ce64e7daee8cfe7b96c88fd" => :mavericks
  end

  depends_on "cmake" => :build

  def install
    system "cmake", ".", *std_cmake_args
    system "make", "install"
    libexec.install "#{buildpath}/examples/benchmarks/dense_blas-bench-cpu" => "test"
  end

  test do
    system "#{opt_libexec}/test"
  end
end
