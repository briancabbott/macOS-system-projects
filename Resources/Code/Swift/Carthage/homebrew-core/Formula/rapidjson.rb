class Rapidjson < Formula
  desc "JSON parser/generator for C++ with SAX and DOM style APIs"
  homepage "https://miloyip.github.io/rapidjson/"
  url "https://github.com/miloyip/rapidjson/archive/v1.1.0.tar.gz"
  sha256 "bf7ced29704a1e696fbccf2a2b4ea068e7774fa37f6d7dd4039d0787f8bed98e"
  license "MIT"
  head "https://github.com/miloyip/rapidjson.git"

  bottle do
    cellar :any_skip_relocation
    sha256 "ab0ca0d82021a22ca8d2e5667f13efc16ea08e18c50e5efadaa1e07ea72f9a31" => :big_sur
    sha256 "07acf5e89f30f9db3b69ad953bf4490ed02f6e5eec9d5f84987962132373aa6e" => :arm64_big_sur
    sha256 "5d8915ade32a25a3c2a973de3536285b2c3d8badd57478c475a9e3eac0f47dc6" => :catalina
    sha256 "9871eeed683c9cb7198c00c87225dd44fc4b40dfa20be2301a63c034ecc221e2" => :mojave
    sha256 "4f40efdbe80e8060d03cfcffdcb2e51d3e4d3924272c96825c6966e00a1ee2e2" => :high_sierra
    sha256 "9fbe96e76e21457931a5e2fff343833b84941e2387ab02212946ad71665c3f6f" => :sierra
    sha256 "d0b949a9bd043535e2ff3e032b45b26de0083d319bc094db7ccc1edfea6cbdb3" => :el_capitan
    sha256 "252ec61e7d5cba129a888bb566d4f2b61bd1bd2886de637f48afa638e6764007" => :yosemite
    sha256 "806e4c788a675bbb0cff3cc9af68f8cdf46ac3d5bf49a47a94b331cc67ca0f4d" => :mavericks
  end

  depends_on "cmake" => :build
  depends_on "doxygen" => :build

  conflicts_with "mesos", because: "mesos installs a copy of rapidjson headers"

  def install
    system "cmake", ".", *std_cmake_args
    system "make", "install"
  end

  test do
    system ENV.cxx, "#{share}/doc/RapidJSON/examples/capitalize/capitalize.cpp", "-o", "capitalize"
    assert_equal '{"A":"B"}', pipe_output("./capitalize", '{"a":"b"}')
  end
end
