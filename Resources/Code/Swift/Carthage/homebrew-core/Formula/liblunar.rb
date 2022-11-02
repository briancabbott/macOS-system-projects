class Liblunar < Formula
  desc "Lunar date calendar"
  homepage "https://code.google.com/archive/p/liblunar/"
  url "https://storage.googleapis.com/google-code-archive-downloads/v2/code.google.com/liblunar/liblunar-2.2.5.tar.gz"
  sha256 "c24a7cd3ccbf7ab739d752a437f1879f62b975b95abcf9eb9e1dd623982bc167"
  revision 1

  bottle do
    sha256 "6ab90c80d7dba5da9f6c5fc702849a6a0c49745832ac67bac0acfac24f745547" => :big_sur
    sha256 "aa7c3b3099b3de258c3080a38afb8bedf0c55d4239c38462ebe66ef6df51d24b" => :arm64_big_sur
    sha256 "916da6f9232b56ff4ba94c1e3aa5457ca6a3a36ad19f91f63cf69fe6503ea7ea" => :catalina
    sha256 "b19be815304a6afc676a11269ca2c520a03e4aed778c4dee20aea063bca69ca8" => :mojave
    sha256 "ce86d50db89030c02c88df1b639fabc9e3c4657e2d954c07b56d4b1cf2bafdf3" => :high_sierra
    sha256 "29252d2fbfdfb582d46a728bd8272767dfa532eb90fedbb67a4b2116749dc657" => :sierra
  end

  depends_on "intltool" => :build
  depends_on "pkg-config" => :build
  depends_on "gettext"
  depends_on "glib"

  def install
    system "./configure", "--disable-dependency-tracking",
                          "--prefix=#{prefix}"
    system "make", "install"
  end
end
