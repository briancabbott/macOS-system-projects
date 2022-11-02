class Cidrmerge < Formula
  desc "CIDR merging with white listing (network exclusion)"
  homepage "https://cidrmerge.sourceforge.io"
  url "https://downloads.sourceforge.net/project/cidrmerge/cidrmerge/cidrmerge-1.5.3/cidrmerge-1.5.3.tar.gz"
  sha256 "21b36fc8004d4fc4edae71dfaf1209d3b7c8f8f282d1a582771c43522d84f088"
  license "GPL-2.0"

  livecheck do
    url :stable
  end

  bottle do
    cellar :any_skip_relocation
    sha256 "5f11e096d4f5b0af52ec6822f2fba79bd053c083b114f41fcc9ca40112daf5db" => :big_sur
    sha256 "49083b84e43debb1921bf1e3788dd5614bcbac3d70b68d099734421fa94f7fd5" => :arm64_big_sur
    sha256 "5828da34c41143336cced7cc8051efd63d525c1a1a4788c6c1235d4bc75cf3df" => :catalina
    sha256 "aa994dfc09a72377c001b0f94a0d8674034fe626e2d1a8bba0d6d514e849564b" => :mojave
    sha256 "61d2b647e77f706f53ef22dcb1ad362d39bed01f2bed08270bc6110824233146" => :high_sierra
    sha256 "8f2cf233141b0ea465c05d3487718176bb40023a05ecf7c275fdae9c36a5eef1" => :sierra
    sha256 "7e607252679cd1648e6c9f48ebbeaa2379ce089ad87815bd6636e65dcedebc7b" => :el_capitan
    sha256 "20c6f57fc6081c8d27d2e68b81e3d4c5cd68e7c799dc30e076f45ee71b42e69d" => :yosemite
    sha256 "89b2d5b31bd190e0aa8837b84f3a684cf01b4501321e898507e5d8dd809f09d7" => :mavericks
  end

  def install
    system "make"
    bin.install "cidrmerge"
  end

  test do
    input = <<~EOS
      10.1.1.0/24
      10.1.1.1/32
      192.1.4.5/32
      192.1.4.4/32
    EOS
    assert_equal "10.1.1.0/24\n192.1.4.4/31\n", pipe_output("#{bin}/cidrmerge", input)
  end
end
