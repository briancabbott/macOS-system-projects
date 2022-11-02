class Tlx < Formula
  desc "Collection of Sophisticated C++ Data Structures, Algorithms and Helpers"
  homepage "https://tlx.github.io"
  url "https://github.com/tlx/tlx/archive/v0.5.20200222.tar.gz"
  sha256 "99e63691af3ada066682243f3a65cd6eb32700071cdd6cfedb18777b5ff5ff4d"
  license "BSL-1.0"

  bottle do
    cellar :any_skip_relocation
    sha256 "bd7aca4d147c132cd31909e0e99f6eb6192d63ba98805d1ae5da6b7ea04826e1" => :big_sur
    sha256 "d08dae9b28680ddbdb24ef98a60110e743d72d702bf8df24eb0568eac35bb536" => :arm64_big_sur
    sha256 "c27858a2595d4fe9444821160e85aa6924fcc7194e13baadd5fda0b79252b9a1" => :catalina
    sha256 "5038cd9dff7968390f0e4208059c02a667fb9c3308ce88f444bd57ef60bd8895" => :mojave
    sha256 "9a81855db3041742ac4e6ae96c3bc8bc9f15e0dc30436afbcbbf36bace3ef633" => :high_sierra
  end

  depends_on "cmake" => :build

  def install
    args = std_cmake_args + [".."]
    mkdir "build" do
      system "cmake", ".", *args
      system "make", "install"
    end
  end

  test do
    (testpath/"test.cpp").write <<~EOS
      #include <tlx/math/aggregate.hpp>
      int main()
      {
        tlx::Aggregate<int> agg;
        for (int i = 0; i < 30; ++i) {
          agg.add(i);
        }
        return 0;
      }
    EOS
    system ENV.cxx, "test.cpp", "-L#{lib}", "-ltlx", "-o", "test", "-std=c++17"
    system "./test"
  end
end
