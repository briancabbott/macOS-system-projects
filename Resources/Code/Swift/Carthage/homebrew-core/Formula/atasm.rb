class Atasm < Formula
  desc "Atari MAC/65 compatible assembler for Unix"
  homepage "https://atari.miribilist.com/atasm/"
  url "https://atari.miribilist.com/atasm/atasm107d.zip"
  version "1.07d"
  sha256 "24a165506346029fbe05ed99b22900ae50f91f5a8c5d38ebad6a92a5c53f3d99"
  license "GPL-2.0"

  bottle do
    cellar :any_skip_relocation
    sha256 "88e14d4779b2bd0437988d9525b5a5f7cca8ce76334bc89780a19acb539de225" => :big_sur
    sha256 "5e039ac5553f6b2dc4e02871041ce1cc7fb4030f90d6601c381ff3060f9c8f2a" => :arm64_big_sur
    sha256 "8f142806b05036e541ef3fec3009d481423f451cbcd99e6be68ae5095cfa205e" => :catalina
    sha256 "7a2437b5a0adf8047fc75a20fb669d2d80b15d261eab0ec0ad5c7d74b9123a2b" => :mojave
    sha256 "b9eb26201949590ab8fce80ee3feabe7f0be2f611e7c60b6b456c8d78480680c" => :high_sierra
  end

  uses_from_macos "zlib"

  def install
    cd "src" do
      system "make", "prog"
      bin.install "atasm"
      system "sed -e 's,%%DOCDIR%%,/usr/local/share/doc/atasm,g' < atasm.1.in > atasm.1"
      man1.install "atasm.1"
    end
    doc.install "examples"
  end

  test do
    cd "#{doc}/examples" do
      system "#{bin}/atasm", "-v", "test.m65", "-o/tmp/test.bin"
    end
  end
end
