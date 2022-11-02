class Rfcmarkup < Formula
  desc "Add HTML markup and links to internet-drafts and RFCs"
  homepage "https://tools.ietf.org/tools/rfcmarkup/"
  url "https://tools.ietf.org/tools/rfcmarkup/rfcmarkup-1.119.tgz"
  sha256 "46c5522f3cba0d430019a60de0e995adbc12f055970b6b341f45181cf8deed8e"

  bottle do
    cellar :any_skip_relocation
    sha256 "da2c561d3972fb09274af264976e07f2eead91603cdfc1084787e8d238a2e05e" => :big_sur
    sha256 "7e74ed6e344dd26f4bb0674b13bf65b9333c29ac7f3552627578b98eb97be599" => :arm64_big_sur
    sha256 "044f47bac55957d49b3665d92e0e33b1bc18a42a9cd465f214b6c08b6838495d" => :catalina
    sha256 "5b39b970ed7615eda4fda38cc597f45b605491e12be8196b2cbe9dacec5e7520" => :mojave
    sha256 "2b9456d420623b967415fdbaf84c9499c852b65a901aed0c4a8e74d286c3af57" => :high_sierra
    sha256 "ceabc26299da811359ca9e1a410e06e9fd3a676249d0501c2436976af1e95462" => :sierra
    sha256 "a15f3c6be0c5eb4b38c4801c6151d4a12b2f206ab6e9c7f11dd0cd94ba7f9e9d" => :el_capitan
    sha256 "5eaeed274aca3e64cbc2407a6b9b531efed736fec325d15109b308bdbea971b4" => :yosemite
    sha256 "5eaeed274aca3e64cbc2407a6b9b531efed736fec325d15109b308bdbea971b4" => :mavericks
  end

  def install
    bin.install "rfcmarkup"
  end

  test do
    system bin/"rfcmarkup", "--help"
  end
end
