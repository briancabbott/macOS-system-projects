class Sic < Formula
  desc "Minimal multiplexing IRC client"
  homepage "https://tools.suckless.org/sic/"
  url "https://dl.suckless.org/tools/sic-1.2.tar.gz"
  sha256 "ac07f905995e13ba2c43912d7a035fbbe78a628d7ba1c256f4ca1372fb565185"
  license "MIT"
  head "https://git.suckless.org/sic", using: :git

  bottle do
    cellar :any_skip_relocation
    sha256 "2c4ab579805b755bf890003ebf105f1af4963890c0d491f221233a365e5cd233" => :big_sur
    sha256 "7547b24c7e3e905ceb4b7b774b9ca7d2c165bd35ad2d4c7cee3908c83c19ed06" => :arm64_big_sur
    sha256 "e8f0a94363bdaebc692584e6a0d2782f88238a9cb4b7920ec6393dcf87d171d8" => :catalina
    sha256 "2c50dd89e57fa0764576417365933792e7599dfb8899ec75957be0fb6d46dd5a" => :mojave
    sha256 "f7e19c7d87f5f13e736edcf7f8cb821b4b644f78208c87f2f6655e5b7541abcc" => :high_sierra
    sha256 "8ec385f1fa892a80c51dca477f469dfe69864d0d5538b652c45ac17914aa5f89" => :sierra
    sha256 "efeb0f7a31a6d4f0ac4c065a4646b5a523788b5edbddd9f99ffa04f00aa41f97" => :el_capitan
    sha256 "99c98bba7ce3793f8f5431cdaee24a0bead3a1a2335bce10dc9cf6d53213c249" => :yosemite
    sha256 "23bfa1932017f0c189e6e3ab1345260d8b5a98697999d6548b9046e7662112db" => :mavericks
  end

  def install
    system "make", "PREFIX=#{prefix}", "install"
  end
end
