class Tractorgen < Formula
  desc "Generates ASCII tractor art"
  homepage "http://www.kfish.org/software/tractorgen/"
  url "http://www.kfish.org/software/tractorgen/dl/tractorgen-0.31.7.tar.gz"
  sha256 "469917e1462c8c3585a328d035ac9f00515725301a682ada1edb3d72a5995a8f"
  license "GPL-2.0"

  bottle do
    cellar :any_skip_relocation
    sha256 "15391ab31cdfcf2c6a844b9e39aed0fabb01e0aa3d03e7a72602f02f0f0e759c" => :big_sur
    sha256 "cc7c0c6f2a31533393973e0931d984d1ceff57e2ee1f49c03a8633d33ecfde7b" => :arm64_big_sur
    sha256 "b28ff1c764b92992d82e16d8ab283215101f3a3aeabcf9aa2d4a952451a779dc" => :catalina
    sha256 "0416b04f09a509f3912de4cac964fb96e2a54246f8ffb9d170d4f2bb16b6f959" => :mojave
    sha256 "936883746158534e9650a0b26f18e680eed527fb56f71ad51e5ec203d8f7f451" => :high_sierra
    sha256 "646d87ca0cb1a5ec93a8aa1ddaa1f28233347ca0a1f56e49c323809ec8295432" => :sierra
    sha256 "ccac503b4577fc81e69d3e778c27c31fad9a1c5fa8627e97f293d87ab1177f8d" => :el_capitan
    sha256 "e50de2fd2d9015873282a62fc7a21f3ef419d527d07eeab3830ace52ec25c3c9" => :yosemite
    sha256 "acbfbe90462924fbc6f2658ca0ee591a122639356ce6ff042b558199b477bf4a" => :mavericks
  end

  def install
    system "./configure", "--prefix=#{prefix}"
    system "make", "install"
  end

  test do
    expected = <<~'EOS'.gsub(/^/, "     ") # needs to be indented five spaces
          r-
         _|
        / |_\_    \\
       |    |o|----\\
       |_______\_--_\\
      (O)_O_O_(O)    \\
    EOS
    assert_equal expected, shell_output("#{bin}/tractorgen 4")
  end
end
