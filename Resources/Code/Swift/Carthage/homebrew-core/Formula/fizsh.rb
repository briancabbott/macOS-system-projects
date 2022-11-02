class Fizsh < Formula
  desc "Fish-like front end for ZSH"
  homepage "https://github.com/zsh-users/fizsh"
  url "https://downloads.sourceforge.net/project/fizsh/fizsh-1.0.9.tar.gz"
  sha256 "dbbbe03101f82e62f1dfe1f8af7cde23bc043833679bc74601a0a3d58a117b07"
  license "BSD-3-Clause"
  head "https://github.com/zsh-users/fizsh.git"

  livecheck do
    url :stable
    regex(%r{url=.*?/fizsh[._-]v?(\d+(?:\.\d+)+(?:-\d+)?)\.t}i)
  end

  bottle do
    cellar :any_skip_relocation
    sha256 "7e97c31a73043c5f5b0119865e63ddba6d0df0aeb87eb19fb4434ac2a5d60ba4" => :big_sur
    sha256 "bd8232b33ec452c52f71dc0507985c2895f6735c2a9a76664f31685f3daa8cb3" => :arm64_big_sur
    sha256 "c1cb674d5a7219ea4c41f3becd0997cf55dfd26a06a17d739f14a3d59da7df75" => :catalina
    sha256 "02457429b5257b916207bc7f46acd5577f8e01183437ef03b594991ba3e69466" => :mojave
    sha256 "7916e571aaf891561a5a6be1ef9708e63ee17ecb41fe60b75129c765d3dad1cb" => :high_sierra
    sha256 "334ceff9d649e87f87be9d3f561ee38221f8c87712a3b506b145191dc51fc4bd" => :sierra
    sha256 "334ceff9d649e87f87be9d3f561ee38221f8c87712a3b506b145191dc51fc4bd" => :el_capitan
    sha256 "334ceff9d649e87f87be9d3f561ee38221f8c87712a3b506b145191dc51fc4bd" => :yosemite
  end

  depends_on "zsh"

  def install
    system "./configure", "--disable-debug",
                          "--disable-dependency-tracking",
                          "--disable-silent-rules",
                          "--prefix=#{prefix}"
    system "make", "install"
  end

  test do
    assert_equal "hello", shell_output("#{bin}/fizsh -c \"echo hello\"").strip
  end
end
