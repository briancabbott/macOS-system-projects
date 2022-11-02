class Mplayershell < Formula
  desc "Improved visual experience for MPlayer on macOS"
  homepage "https://github.com/donmelton/MPlayerShell"
  url "https://github.com/donmelton/MPlayerShell/archive/0.9.3.tar.gz"
  sha256 "a1751207de9d79d7f6caa563a3ccbf9ea9b3c15a42478ff24f5d1e9ff7d7226a"
  license "MIT"
  head "https://github.com/donmelton/MPlayerShell.git"

  bottle do
    cellar :any_skip_relocation
    sha256 "4d08f027c84780edc46b13b1e45a8255de0ec6a35798a1ea5230ef8cb4396e13" => :big_sur
    sha256 "09cfdf5d08af35a3be96623c6535fece3acfbc60cf81247b118778cb2b68acc3" => :catalina
    sha256 "1be2bb2a8eccce7fa190b85af6e67fb7fe36393c32a8295852af0e6e390b6ee9" => :mojave
    sha256 "c0b558e0508d80fe05a1d9617b7aa1986066c54bc0a3da585631eb406da5eb93" => :high_sierra
    sha256 "e9377eaebb65903037105bf3ed6ee301a182452791e9daeaadd08ccb732d9d1b" => :sierra
    sha256 "ae4c1c9d069053afa7e71867256b577e23bd0dec87a90ccab2ebeab089a3634b" => :el_capitan
    sha256 "1637360e180d7b48367cb7c4f01d03856b9d13247000e4cc33f0af5f6ed92101" => :yosemite
    sha256 "a95437813704c56c3e52bd1b17974bec24c209e26df8e9dfe07af45d51ecaf49" => :mavericks
  end

  depends_on xcode: :build

  depends_on "mplayer"

  def install
    xcodebuild "-project",
               "MPlayerShell.xcodeproj",
               "-target", "mps",
               "-configuration", "Release",
               "clean", "build",
               "SYMROOT=build",
               "DSTROOT=build"
    bin.install "build/Release/mps"
    man1.install "Source/mps.1"
  end

  test do
    system "#{bin}/mps"
  end
end
