class Einstein < Formula
  desc "Remake of the old DOS game Sherlock"
  homepage "https://web.archive.org/web/20120621005109/games.flowix.com/en/index.html"
  url "https://web.archive.org/web/20120621005109/games.flowix.com/files/einstein/einstein-2.0-src.tar.gz"
  sha256 "0f2d1c7d46d36f27a856b98cd4bbb95813970c8e803444772be7bd9bec45a548"

  bottle do
    cellar :any
    sha256 "79969ac055b92c9d66391bc912592df75af67046abd57bde81c248a4a68dbfbe" => :big_sur
    sha256 "6499a95257a847e4d273da330d52a71da6dcf4f3de754515eb158ab850f5e0f2" => :arm64_big_sur
    sha256 "54ce5ebd0b06256ecdda309bc0a0b500a0bf29411021fb5525dd647b923c3354" => :catalina
    sha256 "1430c04b154114c5ada29708033872f75a1c1ca361d997747aac748806d0182d" => :mojave
    sha256 "faa76a6c3363ec2c5f814940560db5fb52d8d7af89149dae7bbdf14967c51e3a" => :high_sierra
    sha256 "b2f4290bc28e3dd1c528b7c58fa363f8e5832c00283fa79f2f9243d8e5a02c4c" => :sierra
    sha256 "d0424faaf640750ab3ff8e8e24216a93227b9ff40d33405e3a55a7bdf14d1a36" => :el_capitan
    sha256 "e884bcdb8f1644707fceb03a8d7732a528495e9655216eff42336c64fdd90179" => :yosemite
  end

  depends_on "sdl"
  depends_on "sdl_mixer"
  depends_on "sdl_ttf"

  # Fixes a cast error on compilation
  patch :p0 do
    url "https://raw.githubusercontent.com/Homebrew/formula-patches/85fa66a9/einstein/2.0.patch"
    sha256 "c538ccb769c53aee4555ed6514c287444193290889853e1b53948a2cac7baf11"
  end

  def install
    system "make"

    bin.install "einstein"
    (pkgshare/"res").install "einstein.res"
  end
end
