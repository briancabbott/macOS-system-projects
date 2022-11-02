class Libebur128 < Formula
  desc "Library implementing the EBU R128 loudness standard"
  homepage "https://github.com/jiixyj/libebur128"
  url "https://github.com/jiixyj/libebur128/archive/v1.2.4.tar.gz"
  sha256 "2ee41a3a5ae3891601ae975d5ec2642b997d276ef647cf5c5b363b6127f7add8"
  license "MIT"

  bottle do
    cellar :any
    sha256 "b8cf97cd17ccdc2daa202d568c409c4ce973bd55b68e38b31e2838ff5066c6b7" => :big_sur
    sha256 "a7bf5352cb8a4609f3cc551bd5665c7153ceb9e01694c9b296733bf2451ec077" => :arm64_big_sur
    sha256 "f9bb6bf89d6a32835102ab859e88ab118812ea7920ab1e4416f7ff7f9b2692a6" => :catalina
    sha256 "c51ca6f8e17f7558f35f4c156e7baba1e2658d475dc09eea1b2695e1b1771d42" => :mojave
    sha256 "68c4f6d13808ad4d55d0a0f48384e9872286b6041a06f8c3984ccb96083fcbee" => :high_sierra
    sha256 "d4611c0f7becaf4fbdc34089ddaae18e8017ed6dec859adf4fdadb528f989ae4" => :sierra
    sha256 "3fbb561a893cd7e0858ad25e424e66f70a53023865b33d3519fb1fa62ab35bec" => :el_capitan
  end

  depends_on "cmake" => :build
  depends_on "pkg-config" => :build
  depends_on "speex"

  def install
    system "cmake", ".", *std_cmake_args
    system "make", "install"
  end

  test do
    (testpath/"test.c").write <<~EOS
      #include <ebur128.h>
      int main() {
        ebur128_init(5, 44100, EBUR128_MODE_I);
        return 0;
      }
    EOS
    system ENV.cc, "test.c", "-L#{lib}", "-lebur128", "-o", "test"
    system "./test"
  end
end
