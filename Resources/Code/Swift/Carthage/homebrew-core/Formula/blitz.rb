class Blitz < Formula
  desc "Multi-dimensional array library for C++"
  homepage "https://github.com/blitzpp/blitz/wiki"
  url "https://github.com/blitzpp/blitz/archive/1.0.2.tar.gz"
  sha256 "500db9c3b2617e1f03d0e548977aec10d36811ba1c43bb5ef250c0e3853ae1c2"
  license "Artistic-2.0"
  head "https://github.com/blitzpp/blitz.git"

  bottle do
    cellar :any
    sha256 "eaf888ad2387b3aabccdc8ba82104b942dfa91b058b335449a7bdeb26213ce7d" => :big_sur
    sha256 "0b04264665a05ca8b018a1f9b8e7452297d675b5bb5b50f49af2dd5176de462e" => :arm64_big_sur
    sha256 "2bfa3e5a52f0f51e9e02c84f10f804093b7080c158b3376f330dd51c0f9e3d23" => :catalina
    sha256 "a06052c039592fe7b41face9c72d715ba0602456a9df07a40a472d3ceba02c00" => :mojave
    sha256 "79901f790ea3583942a72ababfba3dc6569169f228b0428c047da52f1f99c02d" => :high_sierra
  end

  depends_on "cmake" => :build

  def install
    mkdir "build" do
      system "cmake", "..", *std_cmake_args
      system "make", "lib"
      system "make", "install"
    end
  end

  test do
    (testpath/"testfile.cpp").write <<~EOS
      #include <blitz/array.h>
      #include <cstdlib>
      using namespace blitz;
      int main(){
        Array<float,2> A(3,1);
        A = 17, 2, 97;
        cout << "A = " << A << endl;
        return 0;}
    EOS
    system ENV.cxx, "testfile.cpp", "-o", "testfile"
    output = shell_output("./testfile")
    var = "/A\ =\ \(0,2\)\ x\ \(0,0\)\n\[\ 17\ \n\ \ 2\ \n\ \ 97\ \]\n\n/"
    assert_match output, var
  end
end
