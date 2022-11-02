class Quvi < Formula
  desc "Parse video download URLs"
  homepage "https://quvi.sourceforge.io/"
  url "https://downloads.sourceforge.net/project/quvi/0.4/quvi/quvi-0.4.2.tar.bz2"
  sha256 "1f4e40c14373cb3d358ae1b14a427625774fd09a366b6da0c97d94cb1ff733c3"
  license "LGPL-2.1"

  livecheck do
    url :stable
    regex(%r{url=.*?/quvi[._-]v?(\d+(?:\.\d+)+)\.t}i)
  end

  bottle do
    cellar :any
    sha256 "1b3252441e8eac802fcd016b09149004b86288c79916e2204be210478af2e185" => :big_sur
    sha256 "195d1401be4ab2b454d97e611163251bb4ed1986cab9c39b089268969fe67ff1" => :arm64_big_sur
    sha256 "403d1157a64341c76067353225c6acbe1c0f3e9c0b69634ed80f0bb6400c4c7c" => :mojave
    sha256 "10fe26a54bcdf8e33e9798b399a3a72e8b571c9668e4398a3f8d1a7952f9c652" => :high_sierra
    sha256 "9e3b86dff84297edec9c63ff1593136c2ce62e8a9f8d523e9d9137943da939bb" => :sierra
    sha256 "c5a8c9b53432e15b4ec31a9c1374bde130d56f73f8ee43e392917a52f34ab945" => :el_capitan
    sha256 "944922426376a9962bb90f032e02ef2404d3155ed3bba81a0b4d349ba1f1aec8" => :yosemite
    sha256 "631889c5bfbfa3741a33efb350b020abaffd163016d375bfa41aedf5cf93262e" => :mavericks
  end

  depends_on "pkg-config" => :build
  depends_on "libquvi"

  def install
    system "./configure", "--disable-dependency-tracking", "--prefix=#{prefix}"
    system "make", "install"
  end

  test do
    system "#{bin}/quvi", "--version"
  end
end
