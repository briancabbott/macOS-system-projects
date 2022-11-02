class SuiteSparse < Formula
  desc "Suite of Sparse Matrix Software"
  homepage "https://people.engr.tamu.edu/davis/suitesparse.html"
  url "https://github.com/DrTimothyAldenDavis/SuiteSparse/archive/v5.8.1.tar.gz"
  sha256 "06726e471fbaa55f792578f9b4ab282ea9d008cf39ddcc3b42b73400acddef40"

  livecheck do
    url :stable
    regex(/^v?(\d+(?:\.\d+)+)$/i)
  end

  bottle do
    sha256 "8c2c7b7b00d0bf8e7da079030a909cac0deb115288b9e610ae97ef375fe22e41" => :big_sur
    sha256 "eac88afe0216d036f170243c08aa0638f6e044d030753ce0c45ad79aa10b4c19" => :arm64_big_sur
    sha256 "0a451d3abea1c06bb2fe629acb97ac970512d4ff583d39ff1264ece13a09a5f4" => :catalina
    sha256 "37a4786b80ef0e2f2ace0b3699ef7a79100ab7f916f5a5b47a558f3e23b6c3de" => :mojave
    sha256 "37d5c3a7863a6f94e087749b97d72a3b0b76f16e0c8ed1d5e71d23b21fec5cc7" => :high_sierra
  end

  depends_on "cmake" => :build
  depends_on "metis"
  depends_on "openblas"
  depends_on "tbb"

  uses_from_macos "m4"

  conflicts_with "mongoose", because: "suite-sparse vendors libmongoose.dylib"

  def install
    mkdir "GraphBLAS/build" do
      system "cmake", "..", *std_cmake_args
    end

    args = [
      "INSTALL=#{prefix}",
      "BLAS=-L#{Formula["openblas"].opt_lib} -lopenblas",
      "LAPACK=$(BLAS)",
      "MY_METIS_LIB=-L#{Formula["metis"].opt_lib} -lmetis",
      "MY_METIS_INC=#{Formula["metis"].opt_include}",
    ]
    system "make", "library", *args
    system "make", "install", *args
    lib.install Dir["**/*.a"]
    pkgshare.install "KLU/Demo/klu_simple.c"
  end

  test do
    system ENV.cc, "-o", "test", pkgshare/"klu_simple.c",
           "-L#{lib}", "-lsuitesparseconfig", "-lklu"
    assert_predicate testpath/"test", :exist?
    assert_match "x [0] = 1", shell_output("./test")
  end
end
