class Fruit < Formula
  desc "Dependency injection framework for C++"
  homepage "https://github.com/google/fruit/wiki"
  url "https://github.com/google/fruit/archive/v3.6.0.tar.gz"
  sha256 "b35b9380f3affe0b3326f387505fa80f3584b0d0a270362df1f4ca9c39094eb5"
  license "Apache-2.0"

  bottle do
    cellar :any
    sha256 "a08a4deb118150ef8237de0dfe4ac6215a729b504f25881950e0113016a9011b" => :big_sur
    sha256 "1efe400614d7043d482a1983f780a4d4792112e039a5939e0fb0e5cdc64d2ad6" => :arm64_big_sur
    sha256 "10f1081e14b11a547b36020cdfa75486fac42036389b37d2df831f586fc78429" => :catalina
    sha256 "fc0a6e56340a21a4548f589d38df7b52bd6edabb483e6c6e0a9fe8605a373a8f" => :mojave
    sha256 "73e0c030fb7984d5b3b72d11410ca2f30c4e5a66ba183070fc3ae8a919ea5094" => :high_sierra
  end

  depends_on "cmake" => :build

  def install
    system "cmake", ".", "-DFRUIT_USES_BOOST=False", *std_cmake_args
    system "make", "install"
    pkgshare.install "examples/hello_world/main.cpp"
  end

  test do
    cp_r pkgshare/"main.cpp", testpath
    system ENV.cxx, "main.cpp", "-I#{include}", "-L#{lib}",
           "-std=c++11", "-lfruit", "-o", "test"
    system "./test"
  end
end
