class Lci < Formula
  desc "Interpreter for the lambda calculus"
  homepage "https://lci.sourceforge.io/"
  url "https://downloads.sourceforge.net/project/lci/lci/0.6/lci-0.6.tar.gz"
  sha256 "204f1ca5e2f56247d71ab320246811c220ed511bf08c9cb7f305cf180a93948e"
  license "GPL-2.0"

  livecheck do
    url :stable
  end

  bottle do
    sha256 "bcdd98bead994b6b6c4551bb89575bfd154dcfff597d94da15a1f6158ca42f11" => :big_sur
    sha256 "10cf9d298c08ffd7dbc86b99853c79013027b0f6e8598f9687d679a312ce3f66" => :arm64_big_sur
    sha256 "a666107791ca81169da0ef403abeb77f5081baa97347d589c49c41c36650db7e" => :catalina
    sha256 "b2bf5e93bb4fdf90b6169b03fe046500fbc196c6d13068160bc92585e66384b3" => :mojave
    sha256 "f6a6d06cb581a5b3c00b05f8785ee33b2c9b69d5b920e8a7234aab3374ca988b" => :high_sierra
    sha256 "207ba1b6eec2e084af44e03c96a25db72fc6fc6a265824a8aad0550bd3178167" => :sierra
    sha256 "88603e7d22fa41138940318a9ce703087062a5466e796a17d636f9ca212a9fc3" => :el_capitan
    sha256 "46a84d5644606edb37c1f915df039901ac96d6728345bb95d57ed52fe783a34d" => :yosemite
    sha256 "d9c2381543f5fff02005a66d07f5c8fb925e4c2c40d87392816947c3c1b22816" => :mavericks
  end

  conflicts_with "lolcode", because: "both install `lci` binaries"

  def install
    system "./configure", "--prefix=#{prefix}"
    system "make", "install"
  end
end
