class Xlslib < Formula
  desc "C++/C library to construct Excel .xls files in code"
  homepage "https://sourceforge.net/projects/xlslib"
  url "https://downloads.sourceforge.net/project/xlslib/xlslib-package-2.5.0.zip"
  mirror "https://dl.bintray.com/homebrew/mirror/xlslib-package-2.5.0.zip"
  sha256 "05a5d052ffdd6590755949d80d16a56285561557bc9a5e887e3b8b3fef92a3f3"

  livecheck do
    url :stable
  end

  bottle do
    cellar :any
    sha256 "a4b1d70f77f5cab84266761845d3910821315696114c3c19250660d4a9bd18a0" => :big_sur
    sha256 "7e4fb2b977db04da50bae5952609b346fb2fd3b2687f2226747c1ff3401f9450" => :arm64_big_sur
    sha256 "9db0f101930faf04be3a8c7cccfafefeb82efc3009e88ab7494296b371631bc3" => :catalina
    sha256 "4cb1f1572aabd2918427724158ef6361390ee0f5268a3c14cb8ecf09a9f7c00d" => :mojave
    sha256 "bb4b5aa643155d211af17a47b5337d65431b1ade0e233af9770d62dbb7ab1448" => :high_sierra
    sha256 "bcdef576e03aa1cad74d341f6fcc72a1e7944a54542941f96cb8ef8063c2190e" => :sierra
    sha256 "a4d5714e19c1d4e44d67bbe9cda064120dc01e9cf207771ae5ef208e76ed2cd9" => :el_capitan
    sha256 "cfd77ea71da12276124cb9cb08bb9ad51ab38b17e912d4dd8c4fec8a428714a1" => :yosemite
    sha256 "b23da4149e9db9bf4152cfc8943ac83d254796d88741592949c3928ae7988149" => :mavericks
  end

  depends_on "autoconf" => :build
  depends_on "automake" => :build
  depends_on "libtool" => :build

  def install
    cd "xlslib"
    system "autoreconf", "-i" # shipped configure hardcodes automake-1.13
    system "./configure", "--disable-debug", "--disable-dependency-tracking",
                          "--prefix=#{prefix}"
    system "make", "install"
  end
end
