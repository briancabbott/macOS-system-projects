class Mbelib < Formula
  desc "P25 Phase 1 and ProVoice vocoder"
  homepage "https://github.com/szechyjs/mbelib"
  url "https://github.com/szechyjs/mbelib/archive/v1.3.0.tar.gz"
  sha256 "5a2d5ca37cef3b6deddd5ce8c73918f27936c50eb0e63b27e4b4fc493310518d"
  license "ISC"
  head "https://github.com/szechyjs/mbelib.git"

  bottle do
    cellar :any
    sha256 "508ed0ed1f9603c7c3e50accea0e201d391f673b63a4acb71574827fddcbb1ef" => :big_sur
    sha256 "053dd044423318deba18dbccbbd1d85efec94b507dd5646beb7b6c3d32064010" => :arm64_big_sur
    sha256 "fb29c40fb9af7c0303d9f7929e61941e8c10c8aad57662f366a671d3a73be116" => :catalina
    sha256 "85f9f705e2e25ea205b637ad34bdc1e3d24734e646e6e6e53d39ab085a691303" => :mojave
    sha256 "710bc1a0458b96c12c0a3b675a3410b1d86257ceb36370fd94952891e1a9b744" => :high_sierra
    sha256 "45f0f9fafbe773fab43f621c62ce0c117c1d9a01fe32528b8b18fa6e94671a22" => :sierra
    sha256 "8cd7158aaccceca6fe78a8031f1d58189b269b0dee86a10c349d3d514c4e33e2" => :el_capitan
    sha256 "30b0fc540b32e244a1cf26719d97a4c57432dbd1537b22a3651fa3d43e1d285b" => :yosemite
    sha256 "3a691e1170cb46a2bc2c8ca3dd456736d5233e9957ec30b9b0bee39b997254d1" => :mavericks
  end

  depends_on "cmake" => :build

  def install
    mkdir "build" do
      system "cmake", "..", *std_cmake_args
      system "make"
      system "make", "test"
      system "make", "install"
    end
  end

  test do
    (testpath/"mb.cpp").write <<~EOS
      extern "C" {
      #include "mbelib.h"
      }
      int main() {
        float float_buf[160] = {1.23f, -1.12f, 4680.412f, 4800.12f, -4700.74f};
        mbe_synthesizeSilencef(float_buf);
        return (float_buf[0] != 0);
      }
    EOS
    system ENV.cxx, "mb.cpp", "-o", "test", "-L#{lib}", "-lmbe"
    system "./test"
  end
end
