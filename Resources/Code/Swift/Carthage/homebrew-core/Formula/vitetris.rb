class Vitetris < Formula
  desc "Terminal-based Tetris clone"
  homepage "https://www.victornils.net/tetris/"
  url "https://github.com/vicgeralds/vitetris/archive/v0.59.1.tar.gz"
  sha256 "699443df03c8d4bf2051838c1015da72039bbbdd0ab0eede891c59c840bdf58d"
  license "BSD-2-Clause"
  head "https://github.com/vicgeralds/vitetris.git", branch: "master"

  bottle do
    cellar :any_skip_relocation
    rebuild 1
    sha256 "1fa572cc6545ae0b7dffcabbab5d15f256c29d0a7d8f8af1bfef4371bf31401c" => :big_sur
    sha256 "4ab846d19502cc5c5aea07435f491a2e7e73f84b37bae0d40e79dffed69a8e6b" => :arm64_big_sur
    sha256 "9b92a065c5c65480ac9fbe8b3414e3c8c467ba6decbe72054a269f18b77e4280" => :catalina
    sha256 "4ff25a3259becb2c40b2f025f30de8fcd269123352764c9d313dfbd2ece6d04f" => :mojave
  end

  def install
    # remove a 'strip' option not supported on OS X and root options for
    # 'install'
    inreplace "Makefile", "-strip --strip-all $(PROGNAME)", "-strip $(PROGNAME)"

    system "./configure", "--prefix=#{prefix}", "--without-xlib"
    system "make", "install"
  end

  test do
    system "#{bin}/tetris", "-hiscore"
  end
end
