class Xtensor < Formula
  desc "Multi-dimensional arrays with broadcasting and lazy computing"
  homepage "https://xtensor.readthedocs.io/en/latest/"
  url "https://github.com/QuantStack/xtensor/archive/0.21.10.tar.gz"
  sha256 "0d1ff8c5f801b566891cfb5c68718f838be94a0b6e07e3fc6164f66e5ec6d3b5"
  license "BSD-3-Clause"

  bottle do
    cellar :any_skip_relocation
    sha256 "225ca1cc6f97582dae87865ab88634bab0073518d7d9b95c954ab9e904bf75f9" => :big_sur
    sha256 "fa1c5a40b0862c89ad0432a8098b45d1f26ba1299a2e0ddc1060948e0bca55db" => :arm64_big_sur
    sha256 "300516fd9600018d31cc79570c690eb2ae9cae811f89fda837fcd093f7e755fa" => :catalina
    sha256 "d2eca50afebcf20a9e602f9ffe1daf485be6dddbb2fab2beac5a779bd53690d6" => :mojave
  end

  depends_on "cmake" => :build

  resource "xtl" do
    url "https://github.com/xtensor-stack/xtl/archive/0.6.20.tar.gz"
    sha256 "fe25d8ae90e2598221f4dd330cfa390efbf256b48ab72a00bc2290b45f3a59cc"
  end

  def install
    resource("xtl").stage do
      system "cmake", ".", *std_cmake_args
      system "make", "install"
    end

    system "cmake", ".", "-Dxtl_DIR=#{lib}/cmake/xtl", *std_cmake_args
    system "make", "install"
  end

  test do
    (testpath/"test.cc").write <<~EOS
      #include <iostream>
      #include "xtensor/xarray.hpp"
      #include "xtensor/xio.hpp"
      #include "xtensor/xview.hpp"

      int main() {
        xt::xarray<double> arr1
          {{11.0, 12.0, 13.0},
           {21.0, 22.0, 23.0},
           {31.0, 32.0, 33.0}};

        xt::xarray<double> arr2
          {100.0, 200.0, 300.0};

        xt::xarray<double> res = xt::view(arr1, 1) + arr2;

        std::cout << res(2) << std::endl;
        return 0;
      }
    EOS
    system ENV.cxx, "-std=c++14", "test.cc", "-o", "test", "-I#{include}"
    assert_equal "323", shell_output("./test").chomp
  end
end
