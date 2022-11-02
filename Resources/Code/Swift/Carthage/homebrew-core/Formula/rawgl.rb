class Rawgl < Formula
  desc "Rewritten engine for Another World"
  homepage "https://github.com/cyxx/rawgl"
  url "https://github.com/cyxx/rawgl/archive/rawgl-0.2.1.tar.gz"
  sha256 "da00d4abbc266e94c3755c2fd214953c48698826011b1d4afbebb99396240073"
  head "https://github.com/cyxx/rawgl.git"

  bottle do
    cellar :any
    rebuild 1
    sha256 "28776c3c1a68e88c5c1418514b745ee71a21d1cb891d26d2c94e2690992e68df" => :big_sur
    sha256 "a54f081334f411e85e8a6eb1c2e6869175d0dd6e5a75df00208e52e6cdbe7151" => :arm64_big_sur
    sha256 "96bd31a9298e14d5b4db183c8833d6e1e6bb10344193f49e1681a13cecc0c276" => :catalina
    sha256 "59d92a845f19239386ea16af01ae174ab61bedeade38b55e492895b55656f576" => :mojave
    sha256 "fb7f71cbce3b517ba8946cea53611c7577a2f1b1618a5f27dd0b67f23e278a25" => :high_sierra
  end

  depends_on "sdl2"
  depends_on "sdl2_mixer"

  # Upstream fix for SDL2_mixer >= 2.0.2
  patch do
    url "https://github.com/cyxx/rawgl/commit/483492fb.patch?full_index=1"
    sha256 "b1f6fb1e4f6ee52a79d128dccd63a7d5a20ced25bb435e934235b679d1299862"
  end

  def install
    system "make"
    bin.install "rawgl"
  end

  test do
    system bin/"rawgl", "--help"
  end
end
