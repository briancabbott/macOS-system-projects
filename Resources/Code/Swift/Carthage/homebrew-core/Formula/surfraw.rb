class Surfraw < Formula
  desc "Shell Users' Revolutionary Front Rage Against the Web"
  homepage "https://packages.debian.org/sid/surfraw"
  url "https://ftp.openbsd.org/pub/OpenBSD/distfiles/surfraw-2.3.0.tar.gz"
  sha256 "ad0420583c8cdd84a31437e59536f8070f15ba4585598d82638b950e5c5c3625"

  bottle do
    cellar :any_skip_relocation
    sha256 "a9e126e0e78269271cee0952d6576fb99c443f49449dc9196a53ee2eb65d7ea6" => :big_sur
    sha256 "65d1418f750b53be50f7d67e98791242056d4d7b5e21ba177899435fd9ac9d0f" => :arm64_big_sur
    sha256 "2a2267217bfdd25ea00b3a08f76c44518e33dac0192a8590e4b3bfa3b5d90073" => :catalina
    sha256 "c9f5fc8020b021799c68cd204d4612f487c44315c15967be78a037576b378920" => :mojave
    sha256 "69920395cbde5fdc2492aa27fc765d4dafe910e26d9d3a05777888425310a0a9" => :high_sierra
    sha256 "69920395cbde5fdc2492aa27fc765d4dafe910e26d9d3a05777888425310a0a9" => :sierra
    sha256 "69920395cbde5fdc2492aa27fc765d4dafe910e26d9d3a05777888425310a0a9" => :el_capitan
  end

  def install
    system "./configure", "--prefix=#{prefix}",
                          "--sysconfdir=#{etc}",
                          "--with-graphical-browser=open"
    system "make"
    ENV.deparallelize
    system "make", "install"
  end

  test do
    output = shell_output("#{bin}/surfraw -p duckduckgo homebrew")
    assert_equal "https://duckduckgo.com/lite/?q=homebrew", output.chomp
  end
end
