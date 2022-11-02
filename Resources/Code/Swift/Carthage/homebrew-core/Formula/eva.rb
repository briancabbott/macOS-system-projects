class Eva < Formula
  desc "Calculator REPL, similar to bc(1)"
  homepage "https://github.com/NerdyPepper/eva/"
  url "https://github.com/NerdyPepper/eva/archive/v0.2.7.tar.gz"
  sha256 "72b2e47e987102d67c9dcbb60e26c4ff0b20e6f844d0d2b9d91c3f073374aee0"
  license "MIT"
  head "https://github.com/NerdyPepper/eva.git"

  bottle do
    cellar :any_skip_relocation
    sha256 "56ca7b33a9a468d35c1124392cd5ea666c739bc8a51b3c160dce58a8435e8896" => :big_sur
    sha256 "fb89730bdb4a21db79cee25cf670c594015066205d7f30107f0b9141512be349" => :arm64_big_sur
    sha256 "351a6e7946aa8597d3c321b47aec89d487f49261146dc1f04cdaadb380f5021a" => :catalina
    sha256 "efed876746895ddcf247f4c90d7b90f1333129d8cc8f789789555045047e1260" => :mojave
    sha256 "706a6ad7ac61b8e8e1247fe1b52ead8c6c5b7b76fe750148a6a38904f3a4ef23" => :high_sierra
  end

  depends_on "rust" => :build

  def install
    system "cargo", "install", *std_cargo_args
  end

  test do
    assert_equal "6.0", shell_output("#{bin}/eva '2 + abs(-4)'").strip
  end
end
