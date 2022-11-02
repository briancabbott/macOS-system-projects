class Aspcud < Formula
  desc "Package dependency solver"
  homepage "https://potassco.org/aspcud/"
  url "https://github.com/potassco/aspcud/archive/v1.9.5.tar.gz"
  sha256 "9cd3a9490d377163d87b16fa1a10cc7254bc2dbb9f60e846961ac8233f3835cf"
  license "MIT"

  bottle do
    sha256 "c934a46742cb4d96d62a3e15dd9e0ada641672f405b1a96edc3f71dac2c87036" => :big_sur
    sha256 "c984eb4cfd2892c3cca7df260064546275a82fa02c01996ee65ecdf8973f27cb" => :arm64_big_sur
    sha256 "3b19a6ee9f466789d05533e5614ae6daf9cf4abc4e2f6347ad401ea7d4d1040a" => :catalina
    sha256 "b5d0df64bc57c7f929b00f04617f9b1260e7c3715abe68b98559c8b693070add" => :mojave
  end

  depends_on "boost" => :build
  depends_on "cmake" => :build
  depends_on "re2c" => :build
  depends_on "clingo"

  def install
    args = std_cmake_args
    args << "-DASPCUD_GRINGO_PATH=#{Formula["clingo"].opt_bin}/gringo"
    args << "-DASPCUD_CLASP_PATH=#{Formula["clingo"].opt_bin}/clasp"

    mkdir "build" do
      system "cmake", "..", *args
      system "make"
      system "make", "install"
    end
  end

  test do
    (testpath/"in.cudf").write <<~EOS
      package: foo
      version: 1

      request: foo >= 1
    EOS
    system "#{bin}/aspcud", "in.cudf", "out.cudf"
  end
end
