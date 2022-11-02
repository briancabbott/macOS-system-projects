class Poco < Formula
  desc "C++ class libraries for building network and internet-based applications"
  homepage "https://pocoproject.org/"
  url "https://pocoproject.org/releases/poco-1.10.1/poco-1.10.1-all.tar.gz"
  sha256 "7f5931e0bb06bc2880a0f3867053a2fddf6c0d3e5dd96342a665460301fc34ca"
  license "BSL-1.0"
  head "https://github.com/pocoproject/poco.git", branch: "develop"

  livecheck do
    url "https://pocoproject.org/releases"
    regex(%r{href=.*?poco[._-]v?(\d+(?:\.\d+)+)/?["' >]}i)
  end

  bottle do
    cellar :any
    sha256 "a2483cf9eaff5857285e2ec3cc4086f74a7edfb240815e75bca7ba153861f1c5" => :big_sur
    sha256 "0755dff1346ea80aa6202ce3e8269c608960abd4bf0a4566e56075cc99364b57" => :catalina
    sha256 "7abccb2c17823c6dda9dee9e5918fa28ef846d8095252681c83c47bbb674f5c8" => :mojave
    sha256 "70cea3a570e187c3e70a8dbbe1ad2e43be1c159d0d9118c1bfc1a8cc6441e2a4" => :high_sierra
  end

  depends_on "cmake" => :build
  depends_on "openssl@1.1"

  def install
    mkdir "build" do
      system "cmake", "..", *std_cmake_args,
                            "-DENABLE_DATA_MYSQL=OFF",
                            "-DENABLE_DATA_ODBC=OFF"
      system "make", "install"
    end
  end

  test do
    system bin/"cpspc", "-h"
  end
end
