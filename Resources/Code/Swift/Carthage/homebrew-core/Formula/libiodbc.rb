class Libiodbc < Formula
  desc "Database connectivity layer based on ODBC. (alternative to unixodbc)"
  homepage "http://www.iodbc.org/dataspace/iodbc/wiki/iODBC/"
  url "https://github.com/openlink/iODBC/archive/v3.52.13.tar.gz"
  sha256 "4bf67fc6d4d237a4db19b292b5dd255ee09a0b2daa4e4058cf3a918bc5102135"
  license any_of: ["BSD-3-Clause", "LGPL-2.0-only"]

  bottle do
    cellar :any
    sha256 "5788f536c0ccce81f9205bc8950d9c158299a3f2339f546192fa695313eb88a7" => :big_sur
    sha256 "35bf9aab3420bf0ba56bfa8802fe1274d397e3a74783f8d1c7c1cb769e3ad83c" => :arm64_big_sur
    sha256 "b9b78f823c2af7962bfc97cb34fd528c8f6eab85823045168ac8ac84eaac3d12" => :catalina
    sha256 "1472bb0987705537158b7c3196d27d01ba02d6c0fdcca733f3cf8d53eca29c5d" => :mojave
    sha256 "77a4fb5fa3036a831e05e2a83585ac2fcdcdf4cf83baa72f28cfb2f8a659ba13" => :high_sierra
    sha256 "abc07f2fe98ed04c4dc5bd5cada2ea68fb9be56337ed442393609f0a22ec21e8" => :sierra
  end

  depends_on "autoconf" => :build
  depends_on "automake" => :build
  depends_on "libtool" => :build

  conflicts_with "unixodbc", because: "both install `odbcinst.h`"

  def install
    system "./autogen.sh"
    system "./configure", "--prefix=#{prefix}"
    system "make", "install"
  end

  test do
    system bin/"iodbc-config", "--version"
  end
end
