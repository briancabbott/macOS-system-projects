class WatchSim < Formula
  desc "Command-line WatchKit application launcher"
  homepage "https://github.com/alloy/watch-sim"
  url "https://github.com/alloy/watch-sim/archive/1.0.0.tar.gz"
  sha256 "138616472e980276999fee47072a24501ea53ce3f7095a3de940e683341b7cba"
  license "MIT"
  head "https://github.com/alloy/watch-sim.git"

  bottle do
    cellar :any_skip_relocation
    sha256 "4dc3e4f2872aeb25d3d4bcb22aac1012730b468543d351d0114498d8211b7f0c" => :big_sur
    sha256 "9a615d042db08236fe150fc21ecc8ac12979007f851b90aa85faa4f7ba75474b" => :arm64_big_sur
    sha256 "bee9797e2c3a52b7dea9b6c5158bb78485b7ee10af530f84d81f31e20babf894" => :catalina
    sha256 "c4f22fd7f296de0c9ace463d4a6b292212178e45a5c483201ee247cc9d33be75" => :mojave
    sha256 "79348a2b95cd3ad0398977a30e46a379cff2b7319941061eebd394342f90d8b2" => :high_sierra
    sha256 "4c0b4b9cf453585d826f3950eba375d8dea80194c50c8d5ab3a014dec9a49c0d" => :sierra
    sha256 "1a7666cc09ecf2228350ea766d0f8e3bd32ab545ac44d9b17b7dc42107c6f15d" => :el_capitan
    sha256 "06d95de04920d991ee1ee5a9e8035fb12ef103aa072382cef82cab683797e8d9" => :yosemite
    sha256 "b04846befdb378679cdd93bdf182784a8ed0da3957cb0b4a124576b3cd06e54c" => :mavericks
  end

  depends_on xcode: "6.2"

  def install
    system "make"
    bin.install "watch-sim"
  end

  test do
    assert_match(/Usage: watch-sim/,
                 shell_output("#{bin}/watch-sim 2>&1", 1))
  end
end
