class Zssh < Formula
  desc "Interactive file transfers over SSH"
  homepage "https://zssh.sourceforge.io/"
  url "https://downloads.sourceforge.net/project/zssh/zssh/1.5/zssh-1.5c.tgz"
  sha256 "a2e840f82590690d27ea1ea1141af509ee34681fede897e58ae8d354701ce71b"
  license "GPL-2.0"

  livecheck do
    url :stable
    regex(%r{url=.*?/zssh[._-]v?(\d+(?:\.\d+)+[a-z]?)\.t}i)
  end

  bottle do
    cellar :any_skip_relocation
    sha256 "6b9bce24c13dd2e979cdae57892e1b595bfcbd1d342bb81419dda378b8439495" => :catalina
    sha256 "0b1567c1d4aef681ff463f058a884eead039fb0c50a1c03820a03c9f67786b52" => :mojave
    sha256 "9cb26f1bd359977406fae945abd311b2cdc5770570e6350f2ac278bfbe458f5b" => :high_sierra
    sha256 "49e01bb86097999f21f3d96b0f9cd63a975d4fd52f6e286d42ceee16ee996eb7" => :sierra
    sha256 "04212f19c1d9a6b97fd56ffe937606f1779849fdf04b93e3f285889599845c8f" => :el_capitan
    sha256 "94280569f9e1c1deb9d8c3be4256cd501399fd51758f8e2ea6d77fd9f1b6ef2e" => :yosemite
    sha256 "94b16bb29616a839134527fd869ac40a8fb5fa88b0048d1a93a828e306c2a270" => :mavericks
  end

  depends_on "lrzsz"

  on_linux do
    depends_on "pkg-config" => :build
    depends_on "readline"
  end

  def install
    system "./configure", "--disable-debug", "--disable-dependency-tracking",
                          "--prefix=#{prefix}"
    system "make"

    bin.install "zssh", "ztelnet"
    man1.install "zssh.1", "ztelnet.1"
  end
end
