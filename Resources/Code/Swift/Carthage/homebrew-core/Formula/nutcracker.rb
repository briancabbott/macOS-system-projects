class Nutcracker < Formula
  desc "Proxy for memcached and redis"
  homepage "https://github.com/twitter/twemproxy"
  url "https://github.com/twitter/twemproxy/archive/v0.4.1.tar.gz"
  sha256 "00c2940f91947bea9457a348316aac1aa1d4e757238aafbefc9d51057da8ede0"
  license "Apache-2.0"
  head "https://github.com/twitter/twemproxy.git"

  bottle do
    cellar :any_skip_relocation
    sha256 "52b41a5455e35d0002ab058b42205c2d0df6a58797e8f5cbf1dc294ededf4ffb" => :big_sur
    sha256 "88e41fafc9595aa7c4b1274d9168b9ef5455393f37ba3ea278b375180d1fd0bd" => :arm64_big_sur
    sha256 "ffac57428129de09b69d1cff52add5e369dc61a7ff70882c44bebe05d1705ae0" => :catalina
    sha256 "e089d40fa4c225d6d8a2f5d7aec22d073fabdcd99d38ad3f99fd292fed534d8a" => :mojave
    sha256 "8d4ab58ba04b1c0622915d080a4b12e6c70b2c338df40ad357b217a6a8ac43e5" => :high_sierra
    sha256 "4987e6e2ef6fffb2fe05a65795cb513d628edda38f0e0ee69cea05159d16a4b5" => :sierra
    sha256 "1f3714c4459185e9002ea87f4fc4a4de429f028df957615ee9876c61a7c89a6d" => :el_capitan
    sha256 "73698710fa026b8585665a9b730626df444dabe6acf118cc4f0c2f57c27e214c" => :yosemite
    sha256 "8e66691c40fe71934bb5eab848c61ef07f8f427774e253c0065eb64cc5410f9b" => :mavericks
  end

  depends_on "autoconf" => :build
  depends_on "automake" => :build
  depends_on "libtool" => :build

  def install
    system "autoreconf", "-ivf"
    system "./configure", "--disable-debug", "--disable-dependency-tracking",
                          "--prefix=#{prefix}"
    system "make", "install"

    pkgshare.install "conf", "notes", "scripts"
  end

  test do
    assert_match version.to_s, shell_output("#{sbin}/nutcracker -V 2>&1")
  end
end
