class Argus < Formula
  desc "Audit Record Generation and Utilization System server"
  homepage "https://qosient.com/argus/"
  url "https://qosient.com/argus/src/argus-3.0.8.2.tar.gz"
  sha256 "ca4e3bd5b9d4a8ff7c01cc96d1bffd46dbd6321237ec94c52f8badd51032eeff"
  license "GPL-3.0"

  livecheck do
    url "https://qosient.com/argus/src/"
    regex(/href=.*?argus[._-]v?(\d+(?:\.\d+)+)\.t/i)
  end

  bottle do
    cellar :any_skip_relocation
    sha256 "c33edf660a14aa03704fe3efda1fb1282b70b127fe881a2402cfa0360a9ea86d" => :big_sur
    sha256 "b3b0421c50d060c2f6812a9b10ee5121336f80306a96271819737903ec98574c" => :arm64_big_sur
    sha256 "8deffdef21a05cf61e3b134532439173966ec8748f1988c4048c3173d6788d2e" => :catalina
    sha256 "83ea7bc0f0103ba900dad6856762aae355f726c0bb9f089cc5434c30dacce1fb" => :mojave
    sha256 "faf6ef808e9ff867eed42586ae6c27f84b66933559e9960fb48853b67325fb20" => :high_sierra
    sha256 "42487c51fa731752e10da402b5fac0f973ee090eaad19f8f4fd52fc5317c9cfb" => :sierra
    sha256 "ea46f2010610e46c120e2df100d61e01c21ee58627e105273c0e0a76437150e1" => :el_capitan
    sha256 "b4d359943e8404d7c6a340c36bbc4d42e14a56cd80e17a997114fdc6f76552d8" => :yosemite
  end

  def install
    system "./configure", "--prefix=#{prefix}"
    system "make"
    system "make", "install"
  end

  test do
    assert_match "Pages", shell_output("#{bin}/argus-vmstat")
  end
end
