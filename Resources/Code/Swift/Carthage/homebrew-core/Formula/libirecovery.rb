class Libirecovery < Formula
  desc "Library and utility to talk to iBoot/iBSS via USB"
  homepage "https://www.libimobiledevice.org/"
  url "https://github.com/libimobiledevice/libirecovery/releases/download/1.0.0/libirecovery-1.0.0.tar.bz2"
  sha256 "cda0aba10a5b6fc2e1d83946b009e3e64d0be36912a986e35ad6d34b504ad9b4"
  license "LGPL-2.1-only"

  bottle do
    cellar :any
    sha256 "4237290aa629bfa59e546e4da6d76d190ca44df8a6205dccf8974541b0d3bc1e" => :big_sur
    sha256 "934427f0de5e9990ca8569960ac0d6cd80f5739401e017b49bb4f79244c953ee" => :arm64_big_sur
    sha256 "a2733550b10ce601236c7e88f8bf689371c42d83e11875459f57a2da8b5bd4e0" => :catalina
    sha256 "09cc0a8c6798d5b9ce0bd08bebdec68ef774f5e3ab4e41837c342c07f888b7bb" => :mojave
    sha256 "04679d947675817c497d74a4a36714ef89a865425c05bc2b936b9bbb9806fe18" => :high_sierra
  end

  head do
    url "https://git.libimobiledevice.org/libirecovery.git"
    depends_on "autoconf" => :build
    depends_on "automake" => :build
    depends_on "libtool" => :build
  end

  def install
    system "./autogen.sh" if build.head?
    system "./configure", "--disable-dependency-tracking",
                          "--disable-silent-rules",
                          "--prefix=#{prefix}",
                          "--enable-debug-code"
    system "make", "install"
  end

  test do
    assert_match "ERROR: Unable to connect to device", shell_output("#{bin}/irecovery -f nothing 2>&1", 255)
  end
end
