class Libmms < Formula
  desc "Library for parsing mms:// and mmsh:// network streams"
  homepage "https://sourceforge.net/projects/libmms/"
  url "https://downloads.sourceforge.net/project/libmms/libmms/0.6.4/libmms-0.6.4.tar.gz"
  sha256 "3c05e05aebcbfcc044d9e8c2d4646cd8359be39a3f0ba8ce4e72a9094bee704f"

  livecheck do
    url :stable
  end

  bottle do
    cellar :any
    rebuild 2
    sha256 "49439ac923403c34c9fb042ed167a8830d424cd113303d66ed2d70f7aeb23840" => :big_sur
    sha256 "23e8d5a9591a26c8f167f346cfd2c789f1241458338eb4ce5f90e9c440aab7f0" => :arm64_big_sur
    sha256 "15016ca7557449405339f310e6feeccbc04094702fcc7d4be53909fc738b05f4" => :catalina
    sha256 "4ac527e54af063a3fa760b1e4d43b56dd51ade89cbb971ac9bea9dd3500dfc70" => :mojave
    sha256 "adc24aaa1656c02f41b20b4453f6a2deda8f3597c919eed1ae8befb732fc920f" => :high_sierra
    sha256 "5319927f799dd20effbfc9f8bb90ebc844b39852c433bf434ab6b36c11c36417" => :sierra
    sha256 "61c4dd24598198386342dd9c700e218b6b83c82627efc781daa89acfaca96066" => :el_capitan
    sha256 "f915d916dd81ad9f767b6905e166dae07df72e70dc0c844c8011abed9f144252" => :yosemite
    sha256 "b55ae55a0d684ba1e3654eee96769d206ce0c22a4ab7bad5241eb1c51bda7778" => :mavericks
  end

  depends_on "pkg-config" => :build
  depends_on "glib"

  def install
    ENV.append "LDFLAGS", "-liconv"
    system "./configure", "--disable-dependency-tracking",
                          "--prefix=#{prefix}"
    system "make", "install"
  end
end
