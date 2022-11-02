class Yamdi < Formula
  desc "Add metadata to Flash video"
  homepage "https://yamdi.sourceforge.io/"
  url "https://downloads.sourceforge.net/project/yamdi/yamdi/1.9/yamdi-1.9.tar.gz"
  sha256 "4a6630f27f6c22bcd95982bf3357747d19f40bd98297a569e9c77468b756f715"

  livecheck do
    url :stable
  end

  bottle do
    cellar :any_skip_relocation
    sha256 "375c99c3793fe45e70a76ef708f9d1b8d5f4e9a7c7f64eca0f7f522926432d82" => :big_sur
    sha256 "f6796b10d1af7ae38ed559313b5646047cb1456c66428d9c32f5e3a0981f1f4c" => :arm64_big_sur
    sha256 "6a3483a957ef3a480f5391d9483b0d3cf8adfce2ec2f6b48289f16733ce29771" => :catalina
    sha256 "228b23059d21ed0a540d3b19c87f3319bb8f99ff57465b8b313d2063660a8567" => :mojave
    sha256 "1c524b6c2d791792b27d15941ecd179b487fbdcd299640f06cbf17bd5f8cf434" => :high_sierra
    sha256 "546a4c5400ef75431ecd3a39dbabda5e5599d82ac3f65f6dafc5d3745a90d8e2" => :sierra
    sha256 "cfaf451a985b0a8cba24a0131c8e0e9a6102eb4b6c315e045ce258999cb19494" => :el_capitan
    sha256 "7041c6dcf877e8e003e2acae68a75ae6a461e92df63fde374157884b52cf2d82" => :yosemite
    sha256 "f0a2a40d6667893a51fb5082a3c09685b688a06df2df8d8b42c350c1dd0f6e16" => :mavericks
  end

  def install
    system ENV.cc, ENV.cflags, "yamdi.c", "-o", "yamdi"
    bin.install "yamdi"
    man1.install "man1/yamdi.1"
  end
end
