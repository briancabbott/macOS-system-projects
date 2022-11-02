class Bogofilter < Formula
  desc "Mail filter via statistical analysis"
  homepage "https://bogofilter.sourceforge.io"
  url "https://downloads.sourceforge.net/project/bogofilter/bogofilter-stable/bogofilter-1.2.5.tar.xz"
  sha256 "3248a1373bff552c500834adbea4b6caee04224516ae581fb25a4c6a6dee89ea"

  livecheck do
    url :stable
  end

  bottle do
    cellar :any
    sha256 "d6ad409edcabed2d32cc945c36151b3a0ae17258d9430f3192b912f1dd1050e8" => :big_sur
    sha256 "2206ad532a38d489deb48bb9cafec00c9b98a09f621f7f208f95cc36387dafb4" => :arm64_big_sur
    sha256 "2f2d4c414683f922e687d054e71619a0455560aac2522484132099fbddcc6a77" => :catalina
    sha256 "d7df5e0d29f4fcbc9eafc129ddfd993dc785ee3a4bf79b70b0dce9b5f31f7be4" => :mojave
    sha256 "c7998fa1651590e6aaf27f8fe014a7b0e305a48a02de4cdcb9ba53f1c84bd1e7" => :high_sierra
  end

  depends_on "berkeley-db"

  def install
    system "./configure", "--disable-dependency-tracking",
                          "--prefix=#{prefix}"
    system "make", "install"
  end

  test do
    system "#{bin}/bogofilter", "--version"
  end
end
