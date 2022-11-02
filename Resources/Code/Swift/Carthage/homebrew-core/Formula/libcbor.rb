class Libcbor < Formula
  desc "CBOR protocol implementation for C and others"
  homepage "http://libcbor.org/"
  url "https://github.com/PJK/libcbor/archive/v0.8.0.tar.gz"
  sha256 "618097166ea4a54499646998ccaa949a5816e6a665cf1d6df383690895217c8b"
  license "MIT"

  bottle do
    cellar :any
    sha256 "3bbc50d56f1418e3acd54c56b28322366b1f1ca306fd3f6fd21755f2848abaab" => :big_sur
    sha256 "0e6a7b38bc44a4fc07643b72b9bdaf91f00e0ae79d4f0539589c9500bb361ac6" => :arm64_big_sur
    sha256 "2860cc48fef2c42aaa50ae25aa90a683e22b81c3905a923e008871679aced20a" => :catalina
    sha256 "105d0a4b3b1a2556603e5e5619a3805183af2041ef06a85784660982c803b97e" => :mojave
    sha256 "5b20e9f902ca71c4f9a1c411c1e65eedb25f9c395d1e3ff691a0d3e7451cd6fd" => :high_sierra
  end

  depends_on "cmake" => :build

  def install
    mkdir "build" do
      system "cmake", "..", "-DWITH_EXAMPLES=OFF", "-DBUILD_SHARED_LIBS=ON", *std_cmake_args
      system "make"
      system "make", "install"
    end
  end

  test do
    (testpath/"example.c").write <<-EOS
    #include "cbor.h"
    #include <stdio.h>
    int main(int argc, char * argv[])
    {
    printf("Hello from libcbor %s\\n", CBOR_VERSION);
    printf("Custom allocation support: %s\\n", CBOR_CUSTOM_ALLOC ? "yes" : "no");
    printf("Pretty-printer support: %s\\n", CBOR_PRETTY_PRINTER ? "yes" : "no");
    printf("Buffer growth factor: %f\\n", (float) CBOR_BUFFER_GROWTH);
    }
    EOS

    system ENV.cc, "-std=c99", "example.c", "-L#{lib}", "-lcbor", "-o", "example"
    system "./example"
    puts `./example`
  end
end
