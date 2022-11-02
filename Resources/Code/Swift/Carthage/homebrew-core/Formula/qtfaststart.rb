class Qtfaststart < Formula
  desc "Utility for Quicktime files"
  homepage "https://libav.org/"
  url "https://libav.org/releases/libav-12.3.tar.gz"
  sha256 "115b659022dd387f662e26fbc5bc0cc14ec18daa100003ffd34f4da0479b272e"
  license "LGPL-2.1"

  livecheck do
    url "https://libav.org/releases/"
    regex(/href=.*?libav[._-]v?(\d+(?:\.\d+)+)\.t/i)
  end

  bottle do
    cellar :any_skip_relocation
    rebuild 1
    sha256 "c9360f4a62eed02d2f5d7879ad4e3c4f6fd250536b3562b228e40d0d3c6c9a70" => :big_sur
    sha256 "e9190e060b5e58f6e294576e87607b43e6f9f6399ae350ba48fb3f522a511c0c" => :arm64_big_sur
    sha256 "abce3f470e0a8b62acd78aa2c58114a3e5b64d7b2117d8ffbaadc23c4eee186e" => :catalina
    sha256 "2fac027c66defdafcbaee5b346fd5c5e6c11b5e9a267de40d604b8e837f5d2c4" => :mojave
    sha256 "073794a6af64b0fe9f2bc22480b4c605f9497c5ae9087d26fa8e51bdc0230b00" => :high_sierra
  end

  resource "mov" do
    url "https://github.com/van7hu/fanca/raw/master/examples/kmplayer/samples/H264_test4_Talkingheadclipped_mov_480x320.mov"
    sha256 "5af004e182ac7214dadf34816086d0a25c7a6cac568ae3741fca527cbbd242fc"
  end

  def install
    system ENV.cc, "-o", "tools/qt-faststart", "tools/qt-faststart.c"
    bin.install "tools/qt-faststart"
  end

  test do
    input = "H264_test4_Talkingheadclipped_mov_480x320.mov"
    output = "out.mov"
    resource("mov").stage testpath
    system "#{bin}/qt-faststart", input, output

    assert_predicate testpath/output, :exist?
  end
end
