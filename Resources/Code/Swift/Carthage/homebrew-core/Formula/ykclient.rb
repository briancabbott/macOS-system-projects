class Ykclient < Formula
  desc "Library to validate YubiKey OTPs against YubiCloud"
  homepage "https://developers.yubico.com/yubico-c-client/"
  license "BSD-2-Clause"
  revision 1

  stable do
    url "https://developers.yubico.com/yubico-c-client/Releases/ykclient-2.15.tar.gz"
    sha256 "f461cdefe7955d58bbd09d0eb7a15b36cb3576b88adbd68008f40ea978ea5016"

    # Use HTTPS and disable old v1 API tests; without this patch "make check" fails
    # to work against api.yubico.com
    patch do
      url "https://github.com/Yubico/yubico-c-client/commit/0d45452e7fbe47e77e78ff23b480c77fd9c06f2b.patch?full_index=1"
      sha256 "1e4e85d9d009488cd81a87f54be0f67a5a4cee03d34e1f0afadbc92f5194e93a"
    end
  end

  bottle do
    cellar :any
    sha256 "9763765c3a3eb3d2fac970755b9c0ed1c48c8d74d7dd767dd824c8b0b3cfffd7" => :big_sur
    sha256 "1a0d524e983bd1b22c012a00d60c4529b776640386e978fb3f6046b52e17eed7" => :catalina
    sha256 "6e1365c8a6d412ae2a65c65b741ab666340bbb384b730989e2833fe96e0b8e92" => :mojave
  end

  head do
    url "https://github.com/Yubico/yubico-c-client.git"

    depends_on "autoconf" => :build
    depends_on "automake" => :build
    depends_on "libtool" => :build
  end

  depends_on "help2man" => :build
  depends_on "pkg-config" => :build

  def install
    system "autoreconf", "-iv" if build.head?
    system "./configure", "--prefix=#{prefix}"
    system "make", "install"
    system "make", "check"
  end

  test do
    assert_equal version.to_s, shell_output("#{bin}/ykclient --version").chomp
  end
end
