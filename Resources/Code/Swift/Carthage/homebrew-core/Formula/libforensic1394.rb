class Libforensic1394 < Formula
  desc "Live memory forensics over IEEE 1394 (\"FireWire\") interface"
  homepage "https://freddie.witherden.org/tools/libforensic1394/"
  url "https://freddie.witherden.org/tools/libforensic1394/releases/libforensic1394-0.2.tar.gz"
  sha256 "50a82fe2899aa901104055da2ac00b4c438cf1d0d991f5ec1215d4658414652e"
  license "LGPL-3.0"
  head "https://github.com/FreddieWitherden/libforensic1394.git"

  bottle do
    cellar :any
    sha256 "f7405930e26a2b4c9cbe939e6121fe0c20fc3b68015f899e91ba4bba75bdbdbb" => :big_sur
    sha256 "e199817199f736890486c3667735acf01876563584bdcde56b2671ac85707ba9" => :arm64_big_sur
    sha256 "4b9746197d1e43ee78530b552e42a6e6cc96908267db1685076f6e283983ab0f" => :catalina
    sha256 "90e2abdc0baef51cfb97b8f9d130f99e2ad5e1eda990f7e6fc29c0e0d1e2f79a" => :mojave
    sha256 "5e919cf8bce0747630324f0c203bbd1aef4d7e17d278f42bcbece48da2229c8f" => :high_sierra
    sha256 "e747c5c6797d48070c4a4199fe38021cd0164a052e14b21005b9caf4a47a6e3c" => :sierra
    sha256 "d850e7c3a04b206c6219c75ba0a00723e9a25d0c97831de289320ef0cc076aae" => :el_capitan
    sha256 "b64837090b557e25444999bfc41e2023f8fc2ced465ef7ccc067938fe0ec2f2c" => :yosemite
    sha256 "a2039bd1c161253eac9ac123414f660fbb6059f709b6499ec6a92c73bfba4f42" => :mavericks
  end

  depends_on "cmake" => :build

  def install
    system "cmake", ".", *std_cmake_args
    system "make", "install"
  end

  test do
    (testpath/"test.c").write <<~EOS
      #include <assert.h>
      #include <forensic1394.h>
      int main() {
        forensic1394_bus *bus;
        bus = forensic1394_alloc();
        assert(NULL != bus);
        forensic1394_destroy(bus);
        return 0;
      }
    EOS
    system ENV.cc, "test.c", "-L#{lib}", "-lforensic1394", "-o", "test"
    system "./test"
  end
end
