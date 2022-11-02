class Dynamips < Formula
  desc "Cisco 7200/3600/3725/3745/2600/1700 Router Emulator"
  homepage "https://github.com/GNS3/dynamips"
  url "https://github.com/GNS3/dynamips/archive/v0.2.21.tar.gz"
  sha256 "08587589db2c3fc637e6345aebf4f9706825c12f45d9e2cf40d4189c604656d2"
  license "GPL-2.0"

  livecheck do
    url :stable
    strategy :github_latest
  end

  bottle do
    cellar :any_skip_relocation
    sha256 "1cf10fcefc4f6d8d2fff03b407e0d8b2730ef944c9d91762823ca1865b7b3f33" => :big_sur
    sha256 "ecf536589504e42389e91865495faa4eb30ba20adad56bc865c0481e80abefe4" => :catalina
    sha256 "db5398464afdb11af6f26cd4780f6e688bed0f35c9fea8f8308f11991987a037" => :mojave
    sha256 "cb9bf6eebd6a7987976e0e2543a807e1b0f16698a1c71eb64e7da56f320fd425" => :high_sierra
    sha256 "08b44502cd3b052592f11f5b75453fabd51fdcfe1a311405c4b7329a701dc424" => :sierra
  end

  depends_on "cmake" => :build
  depends_on "libelf"

  def install
    ENV.append "CFLAGS", "-I#{Formula["libelf"].include}/libelf"

    ENV.deparallelize
    system "cmake", ".", "-DANY_COMPILER=1", *std_cmake_args
    system "make", "DYNAMIPS_CODE=stable",
                   "DYNAMIPS_ARCH=amd64",
                   "install"
  end

  test do
    system "#{bin}/dynamips", "-e"
  end
end
