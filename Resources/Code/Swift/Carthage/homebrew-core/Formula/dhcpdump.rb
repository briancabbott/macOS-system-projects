class Dhcpdump < Formula
  desc "Monitor DHCP traffic for debugging purposes"
  homepage "https://www.mavetju.org/"
  url "https://www.mavetju.org/download/dhcpdump-1.8.tar.gz"
  mirror "https://deb.debian.org/debian/pool/main/d/dhcpdump/dhcpdump_1.8.orig.tar.gz"
  sha256 "6d5eb9418162fb738bc56e4c1682ce7f7392dd96e568cc996e44c28de7f77190"

  bottle do
    cellar :any_skip_relocation
    sha256 "c6661ad77e8aa1a0355ca8c93454e9a5cba9936790b2fdbf97ada7c7e094af1a" => :big_sur
    sha256 "ea331a3bb132f7f9fbc334e85fd7d7cfc6cf7314df93bf123c5e042febbe1951" => :arm64_big_sur
    sha256 "508365f542d3fc6574525a2b2ccacc4467b40fd53b9885e8c4e91e9e2b172f21" => :catalina
    sha256 "5c0e4a3120148871209e5943dd42c5fc81ad6b8d0e78e0964d2dc46829ac5030" => :mojave
    sha256 "d49aaa82bf92fd7adeb0edb91812d4c48aa20fb0af2f30f9b4238d032dbb99ce" => :high_sierra
    sha256 "10e6565cdd5717666822ce9a0f77747d44969b5cbc7c3ccf1505aaa6ba95af85" => :sierra
    sha256 "1f30cb4146a741b3523d674336854c665546e939af04f619e38623d9298cd4ef" => :el_capitan
    sha256 "6df64653cfd4b118db43e2acb2f08a565ac3cba9e1b739a258eeb7655c1a6103" => :yosemite
    sha256 "c549cb96db8e621b379b1a02cd4e743ff74b4d212f2d880649351e28c4c63684" => :mavericks
  end

  def install
    system "make"
    bin.install "dhcpdump"
    man8.install "dhcpdump.8"
  end

  test do
    system "#{bin}/dhcpdump", "-h"
  end
end
