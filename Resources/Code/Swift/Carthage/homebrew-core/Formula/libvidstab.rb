class Libvidstab < Formula
  desc "Transcode video stabilization plugin"
  homepage "http://public.hronopik.de/vid.stab/"
  url "https://github.com/georgmartius/vid.stab/archive/v1.1.0.tar.gz"
  sha256 "14d2a053e56edad4f397be0cb3ef8eb1ec3150404ce99a426c4eb641861dc0bb"
  license "GPL-2.0-or-later"

  bottle do
    cellar :any
    rebuild 1
    sha256 "b4c67e80b92e95aa19520b0b130a60cc3949db7899d9d02520d32d9fc62ec837" => :big_sur
    sha256 "b98be46d2375a1e6b30947b31c981009785a7c0e97c31ca0c64a52228b0d1576" => :arm64_big_sur
    sha256 "df23e5e7933b6535f34c429ee8286e4d9dec6d0a2349cf3256f44ec687e7968f" => :catalina
    sha256 "783224577a1cc7a57de76eac74b00aac69e7fe15c920d26454e58a369854974f" => :mojave
    sha256 "d3a80889cbeaa5a8af0abc5037c35afefb181e902b79f4f986a6b4c4e29d88a5" => :high_sierra
  end

  depends_on "cmake" => :build

  # A bug in the FindSSE CMake script means that, if a variable is defined
  # as an empty string without quoting, it doesn't get passed to a function
  # and CMake throws an error. This only occurs on ARM, because the
  # sysctl value being checked is always a non-empty string on Intel.
  # Upstream PR: https://github.com/georgmartius/vid.stab/pull/93
  patch do
    url "https://raw.githubusercontent.com/Homebrew/formula-patches/5bf1a0e0cfe666ee410305cece9c9c755641bfdf/libvidstab/fix_cmake_quoting.patch"
    sha256 "45c16a2b64ba67f7ca5335c2f602d8d5186c29b38188b3cc7aff5df60aecaf60"
  end

  def install
    system "cmake", ".", "-DUSE_OMP=OFF", *std_cmake_args
    system "make", "install"
  end
end
