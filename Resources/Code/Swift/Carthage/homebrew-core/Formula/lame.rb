class Lame < Formula
  desc "High quality MPEG Audio Layer III (MP3) encoder"
  homepage "https://lame.sourceforge.io/"
  url "https://downloads.sourceforge.net/project/lame/lame/3.100/lame-3.100.tar.gz"
  sha256 "ddfe36cab873794038ae2c1210557ad34857a4b6bdc515785d1da9e175b1da1e"
  license "LGPL-2.0"

  livecheck do
    url :stable
    regex(%r{url=.*?/lame[._-]v?(\d+(?:\.\d+)+)\.t}i)
  end

  bottle do
    cellar :any
    sha256 "6ceaf88479ce365df8c29140359984ad8debcc44898b99424b39d729e923279b" => :big_sur
    sha256 "2ff2c6ad3cfd26e1ba53230631e2f04734a4638c344cce50ff0b8fc36b45c403" => :arm64_big_sur
    sha256 "02b6a2cbf9b902225308bc90c8314699761cbdcd13628271579f5345d8160af2" => :catalina
    sha256 "737751faa513a68ac2499bb5cc607bc366e15dab8ff3bff5443567a455af5c3f" => :mojave
    sha256 "9e65c67b83efa5a686aea0506dc44935cd2af2d4fe55fe38dc19610a0ccd80dd" => :high_sierra
    sha256 "c2d7bce53be2efb5d19d99ea00fbe69613885cce46009e8ab6099f8d5925c3ba" => :sierra
    sha256 "73c4d677b4e5357dc5baf30c96ac5f33cf7902e9c77869834b7cd9d17f3415bc" => :el_capitan
  end

  uses_from_macos "ncurses"

  def install
    # Fix undefined symbol error _lame_init_old
    # https://sourceforge.net/p/lame/mailman/message/36081038/
    inreplace "include/libmp3lame.sym", "lame_init_old\n", ""

    system "./configure", "--disable-dependency-tracking",
                          "--disable-debug",
                          "--prefix=#{prefix}",
                          "--enable-nasm"
    system "make", "install"
  end

  test do
    system "#{bin}/lame", "--genre-list", test_fixtures("test.mp3")
  end
end
