class Mdr < Formula
  desc "Make diffs readable"
  homepage "https://github.com/halffullheart/mdr"
  url "https://github.com/halffullheart/mdr/archive/v1.0.1.tar.gz"
  sha256 "103d52c47133a43cc7a6cb8a21bfabe2d6e35e222d5b675bc0c868699a127c67"
  license "GPL-3.0"

  bottle do
    cellar :any_skip_relocation
    sha256 "4540fb82156ec6317ae37ffc8889b1e11d6a0b6327528e53bca505057632c31f" => :big_sur
    sha256 "4faacaab5dd0acefaee74a73abaa093d69bd6caefb764375d0565f20605b81c6" => :arm64_big_sur
    sha256 "9da0233ef931bc31dff9356e3298f5c838fbbe3422d64cbfa1e3751bd09545d0" => :catalina
    sha256 "6dec04545f16f59af2b9b2397d4ebf65c204c827fef52cb20ef81c12d2273cda" => :mojave
    sha256 "58d0fa82a0e6291d934bbc3f12f586fbb35282f9d15db017126e042f209dd664" => :high_sierra
    sha256 "ef68c4389ee92beeb6c04e1859f398d108ffcce03fe692dd5776f7e12d776672" => :sierra
    sha256 "0b522120151f1116ae7e681ff2fb129ecd26486202ca753d6b1de902f6f29334" => :el_capitan
    sha256 "7048e71ef8f9a1d5c1712dce6cb33df08029038d771789021a1b8bc1e5f4ad10" => :yosemite
    sha256 "b80b64d56e7e77e9b53dd8c308dd50450552b782a72204cb710adf2de28c4f9e" => :mavericks
  end

  def install
    system "rake"
    libexec.install Dir["*"]
    bin.install_symlink libexec/"build/dev/mdr"
  end

  test do
    system "#{bin}/mdr", "-h"
  end
end
