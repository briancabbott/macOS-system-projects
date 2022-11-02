class Avce00 < Formula
  desc "Make Arc/Info (binary) Vector Coverages appear as E00"
  homepage "http://avce00.maptools.org/avce00/index.html"
  url "http://avce00.maptools.org/dl/avce00-2.0.0.tar.gz"
  sha256 "c0851f86b4cd414d6150a04820491024fb6248b52ca5c7bd1ca3d2a0f9946a40"

  bottle do
    cellar :any_skip_relocation
    sha256 "1fdd45d6a401ca88019bbd58cb3afdda23dedf706c4556e87a7cc48b1a3e952a" => :big_sur
    sha256 "9c4e10ddc6cad4b6a7001a697b8318513a09166e70fa6831ed5fdbede1e71d47" => :arm64_big_sur
    sha256 "db71ee14a03d041413530c0974ce7703100dc3259fc0d2ea5a32fadcf7180133" => :catalina
    sha256 "285b4eb74ff189689097df36f62fa4c2a48b68ece7442156a5700b6c36feb743" => :mojave
    sha256 "40b26638adfaf290bc07ae792da49106b493ea3109a97c1fac775723a0463ac4" => :high_sierra
    sha256 "576b5ea62376b42733d56e7bd862522588c16160ac1abb5f382c1c12055248e1" => :sierra
    sha256 "45f18e289431af4de0d1e96c1fadd6a056e80907a1650654f8ee0dd1dafab401" => :el_capitan
    sha256 "56e15b29411b2947d9a842d91ae713e16566aa59e297e06f7d4de4b301847e66" => :yosemite
    sha256 "55990b93f7fe4639c6fdf29c4cc6c5791c6178c8661e22ef9e0dd64606532f56" => :mavericks
  end

  conflicts_with "gdal", because: "both install a cpl_conv.h header"

  def install
    system "make", "CC=#{ENV.cc}"
    bin.install "avcimport", "avcexport", "avcdelete", "avctest"
    lib.install "avc.a"
    include.install Dir["*.h"]
  end

  test do
    touch testpath/"test"
    system "#{bin}/avctest", "-b", "test"
  end
end
