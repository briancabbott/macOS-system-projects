class Wagyu < Formula
  desc "Rust library for generating cryptocurrency wallets"
  homepage "https://github.com/AleoHQ/wagyu"
  url "https://github.com/AleoHQ/wagyu/archive/v0.6.1.tar.gz"
  sha256 "2458b3d49653acd5df5f3161205301646527eca9f6ee3d84c7871afa275bad9f"
  head "https://github.com/AleoHQ/wagyu.git"

  bottle do
    cellar :any_skip_relocation
    rebuild 1
    sha256 "2b818f64c4f18e5fc7694d89f1bc1d038a0095ede55e3a5ecec2c80a0a04fa1d" => :big_sur
    sha256 "69e6539d7e3801aaea4cd14acd48684f703a4c1cac0f04790d3ada827daf77f9" => :catalina
    sha256 "0b6fd9b45280ecac2586b191303e0e643ef14c85cad06b6aca73e51e7af6ae46" => :mojave
    sha256 "c2175413a53a69da950ca7b879afc882f2181a34cb633e823bf2a3dc29675fc4" => :high_sierra
  end

  depends_on "rust" => :build

  def install
    system "cargo", "install", *std_cargo_args
  end

  test do
    system "#{bin}/wagyu", "bitcoin"
  end
end
