class Arpoison < Formula
  desc "UNIX arp cache update utility"
  homepage "http://www.arpoison.net/"
  url "http://www.arpoison.net/arpoison-0.7.tar.gz"
  sha256 "63571633826e413a9bdaab760425d0fab76abaf71a2b7ff6a00d1de53d83e741"
  revision 1

  livecheck do
    url :homepage
    regex(/href=.*?arpoison[._-]v?(\d+(?:\.\d+)+)\.t/i)
  end

  bottle do
    cellar :any
    sha256 "2009a1bff74b3d6e4fd4eb5f76ce104473e1c322e2f666cf3f5962de2bc99a0c" => :big_sur
    sha256 "376ce845d964f61c095ab7a16d2645d3688ebf5810b18dfb8badb8a24da0e66f" => :arm64_big_sur
    sha256 "550588e02ce0eb78b47d2d2f9e8b863c29761667aca72e4ad0c0810b13682d9b" => :catalina
    sha256 "c97bb55590119dbda338a24e634f9089bb3e43889a810a7bece231d6304b7bcf" => :mojave
    sha256 "ee2eedf6780546bcf4610984d36a773300c5528122d08b7873b640a51f76ee56" => :high_sierra
  end

  depends_on "libnet"

  def install
    system "make"
    bin.install "arpoison"
    man8.install "arpoison.8"
  end

  test do
    # arpoison needs to run as root to do anything useful
    assert_match "target MAC", shell_output("#{bin}/arpoison", 1)
  end
end
