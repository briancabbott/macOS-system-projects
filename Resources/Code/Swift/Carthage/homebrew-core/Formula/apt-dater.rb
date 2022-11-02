class AptDater < Formula
  desc "Manage package updates on remote hosts using SSH"
  homepage "https://github.com/DE-IBH/apt-dater"
  url "https://github.com/DE-IBH/apt-dater/archive/v1.0.4.tar.gz"
  sha256 "a4bd5f70a199b844a34a3b4c4677ea56780c055db7c557ff5bd8f2772378a4d6"
  license "GPL-2.0"
  revision 1
  version_scheme 1

  bottle do
    sha256 "cf4a97e076ce5f8820c9a1dc787c5e751b350cc223d17ec0ba6007d6e8d97484" => :big_sur
    sha256 "ae020a711348a85409b5fa30467b329b1e009c006029809da302e9dc89bbee40" => :arm64_big_sur
    sha256 "5fe58574f889c5e29bd2f4c492848281450da398cace807a33c5100b44090665" => :catalina
    sha256 "d736fdabb393e90e6895b9d5694cc0a78f592bd363483e7e935d044fd0331d41" => :mojave
    sha256 "f6b5f606925ac38d24ef56fc52e93c3f5a4e8f1ab2d687ebb376c78d4f91f366" => :high_sierra
    sha256 "66d81a3bf524ab635a34803119837ef26704011b2d362ab7f41aba0d40b54ea3" => :sierra
  end

  depends_on "autoconf" => :build
  depends_on "automake" => :build
  depends_on "pkg-config" => :build
  depends_on "gettext"
  depends_on "glib"
  depends_on "popt"

  uses_from_macos "libxml2"

  def install
    system "autoreconf", "-ivf"
    system "./configure", "--disable-debug",
                          "--disable-dependency-tracking",
                          "--prefix=#{prefix}"
    system "make", "install"
    # Global config overrides local config, so delete global config to prioritize the
    # config in $HOME/.config/apt-dater
    (prefix/"etc").rmtree
  end

  test do
    system "#{bin}/apt-dater", "-v"
  end
end
