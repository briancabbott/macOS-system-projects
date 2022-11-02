class Afio < Formula
  desc "Creates cpio-format archives"
  homepage "https://github.com/kholtman/afio"
  url "https://github.com/kholtman/afio/archive/v2.5.2.tar.gz"
  sha256 "c64ca14109df547e25702c9f3a9ca877881cd4bf38dcbe90fbd09c8d294f42b9"
  head "https://github.com/kholtman/afio.git"

  bottle do
    cellar :any_skip_relocation
    rebuild 1
    sha256 "0daf7df23f36271e3141cc11cab067b33ed5855b9faba53bc697d5259deb82ca" => :big_sur
    sha256 "ca63097a9d1a29c00ae8a799941e937c7359b9df59c723b6110cd7b5cfe7c943" => :arm64_big_sur
    sha256 "28494133d10acea2c1a298fe858d26889ba8567422b9f431710b156a4a8ac858" => :catalina
    sha256 "733a4169a7be82dc173cc302994ad205493a9085580634b92faa38c96c84608b" => :mojave
    sha256 "53dbb826f2c3e050bd70078945d92772a4c434b0aa75e1a71cb29e56ed8e62fa" => :high_sierra
  end

  def install
    system "make", "DESTDIR=#{prefix}"
    bin.install "afio"
    man1.install "afio.1"

    prefix.install "ANNOUNCE-2.5.1" => "ANNOUNCE"
    prefix.install %w[INSTALLATION SCRIPTS]
    share.install Dir["script*"]
  end

  test do
    path = testpath/"test"
    path.write "homebrew"
    pipe_output("#{bin}/afio -o archive", "test\n")

    system "#{bin}/afio", "-r", "archive"
    path.unlink

    system "#{bin}/afio", "-t", "archive"
    system "#{bin}/afio", "-i", "archive"
    assert_equal "homebrew", path.read.chomp
  end
end
