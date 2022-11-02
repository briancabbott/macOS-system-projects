class Libdca < Formula
  desc "Library for decoding DTS Coherent Acoustics streams"
  homepage "https://www.videolan.org/developers/libdca.html"
  url "https://download.videolan.org/pub/videolan/libdca/0.0.7/libdca-0.0.7.tar.bz2"
  sha256 "3a0b13815f582c661d2388ffcabc2f1ea82f471783c400f765f2ec6c81065f6a"
  license "GPL-2.0"

  livecheck do
    url "https://download.videolan.org/pub/videolan/libdca/"
    regex(%r{href=.*?v?(\d+(?:\.\d+)+)[/"'>]}i)
  end

  bottle do
    cellar :any
    sha256 "123d7863f98b6fc1f56aaca440db706764b43c99fe1a5bd5286badf160f76d62" => :big_sur
    sha256 "d20b5e52384fcbb0da4501eb109e3aac6be3eb6f0e6a8f09de0c61b2f3c83361" => :arm64_big_sur
    sha256 "d9c4b3a350744867f5782db738d25d1212b9be89449030492083364574f914d7" => :catalina
    sha256 "594d6b26eb3ca16c3046ff2792de4f78a0f038dc94b1972c8827e86331a46fde" => :mojave
    sha256 "f8ba469ce443efa0e9fc87b51a87c6b4d510bd3e7bb91ae11d1f91e99f760acc" => :high_sierra
  end

  depends_on "autoconf" => :build
  depends_on "automake" => :build
  depends_on "libtool" => :build

  def install
    # Fixes "duplicate symbol ___sputc" error when building with clang
    # https://github.com/Homebrew/homebrew/issues/31456
    ENV.append_to_cflags "-std=gnu89"

    system "autoreconf", "-fiv"
    system "./configure", "--disable-debug", "--disable-dependency-tracking",
                          "--prefix=#{prefix}"
    system "make"
    system "make", "install"
  end
end
