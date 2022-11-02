class CrushTools < Formula
  desc "Command-line tools for processing delimited text data"
  homepage "https://github.com/google/crush-tools"
  url "https://github.com/google/crush-tools/releases/download/20150716/crush-tools-20150716.tar.gz"
  sha256 "ef2f9c919536a2f13b3065af3a9a9756c90ede53ebd67d3e169c90ad7cd1fb05"
  license "Apache-2.0"

  bottle do
    cellar :any
    sha256 "1e4c85501024bcfef0641f3e16abd540597f68accc5118b6da5a7f24218fba00" => :big_sur
    sha256 "c126bcdab0ba561df10768d423dd6b9336e7464707e3af7c17e65178cbffd4f5" => :arm64_big_sur
    sha256 "c5172b5ab0e1d85d0e1e87e0dc83b66b5ee8ffda0d86f85f586e4e8850268861" => :catalina
    sha256 "148684da73eef05ce20f602fdc2d0a9795afbdb6db9cd324c74860c6600ff835" => :mojave
    sha256 "729196f80c05c5e395c145752a7a54cc1488a6cf1767b43ed9f639c2f3f3c463" => :high_sierra
    sha256 "28286e04a7baf7790f446f2f474a74387e6c0282df70d89ade39c84187394ce4" => :sierra
    sha256 "0c7c58b9f2ec87237934eda55932b200c6d7b7f6dbb07a35e0a49ed389e984d3" => :el_capitan
    sha256 "90c901bd6daf8178407232c6b3be7f3c5056e9cf2ab88750d09b151e0973d4ff" => :yosemite
    sha256 "f1319787a7aafc6610f0217299791c428e5784d11cc93c8cd623e8a5cba5c414" => :mavericks
  end

  head do
    url "https://github.com/google/crush-tools.git"
    depends_on "autoconf" => :build
    depends_on "automake" => :build
    depends_on "libtool" => :build
  end

  depends_on "pcre"

  conflicts_with "aggregate", because: "both install an `aggregate` binary"
  conflicts_with "num-utils", because: "both install an `range` binary"

  def install
    system "./bootstrap" if build.head?
    system "./configure", "--disable-dependency-tracking",
                          "--prefix=#{prefix}"
    system "make", "install"
  end

  test do
    assert_equal "1 2 6 7 8 9 10", shell_output("#{bin}/range 1,2,6-10").strip
    assert_equal "o", shell_output("#{bin}/tochar 111")
  end
end
