class Ecm < Formula
  desc "Prepare CD image files so they compress better"
  homepage "https://web.archive.org/web/20140227165748/www.neillcorlett.com/ecm/"
  url "https://web.archive.org/web/20091021035854/www.neillcorlett.com/downloads/ecm100.zip"
  version "1.0"
  sha256 "1d0d19666f46d9a2fc7e534f52475e80a274e93bdd3c010a75fe833f8188b425"

  # The first-party web page was been missing since 2014, so we can't check for
  # new versions and the developer doesn't seem to be actively working on this.
  livecheck do
    skip "No available sources to check for versions"
  end

  bottle do
    cellar :any_skip_relocation
    sha256 "efe66585b67b58f533f0657def18a8657b8f46c3971c80d44e49f1241c7342da" => :big_sur
    sha256 "868674e4445ccdd9ccee5c57a7acdee0fd1de28bdd1a9fa1405dbc3c265d9bfa" => :arm64_big_sur
    sha256 "93a81fd556f0fa5cb39016f88a50aa7880b65d7f281cd94fbdaba36320d33f1d" => :catalina
    sha256 "0ee8841213cd082cb18043cbc1bb6d38563abc05f5477906649b68665e82539f" => :mojave
    sha256 "985409c27f6eff735bda8566c07ee787fd2ed4c92ac72c303376076d14e08dd9" => :high_sierra
    sha256 "888612dee7486ca5413e2b1e0090a4e1bd5ea7f2fe5cc53fe02bb326ed4f3d4c" => :sierra
    sha256 "3ecb325a368ef42737e77003e9ecc13a8d402a34da3a25c039b36565fef0b55d" => :el_capitan
    sha256 "9eef5eb54af2ad50ab05ee9382efe8d0ca831a6d058fe1fa2679cff87aa0a064" => :yosemite
    sha256 "c80a78299a5042d8588806066af2b03e9a3a679e8284fb863d9ee15edc690933" => :mavericks
  end

  def install
    system ENV.cc, "-o", "ecm", "ecm.c"
    system ENV.cc, "-o", "unecm", "unecm.c"
    bin.install "ecm", "unecm"
  end
end
