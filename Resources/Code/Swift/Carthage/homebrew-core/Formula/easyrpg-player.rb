class EasyrpgPlayer < Formula
  desc "RPG Maker 2000/2003 games interpreter"
  homepage "https://easyrpg.org/"
  url "https://easyrpg.org/downloads/player/0.6.2.3/easyrpg-player-0.6.2.3.tar.xz"
  sha256 "6702b78949b26aeb6d1e26dbffa33f6352ca14111774bfd433bc140c146087d0"
  license "GPL-3.0-or-later"

  livecheck do
    url "https://github.com/EasyRPG/Player.git"
  end

  bottle do
    cellar :any
    sha256 "481bef5afabcabe0f34eea0586fe8134161bab387c87a32fcc3ca77e69063189" => :big_sur
    sha256 "60df6f5b55d829c737c6f534a85db694ca4469586284cd22cab9d50613bda89f" => :arm64_big_sur
    sha256 "e6b485bfe87e67da97b5bc34c828889286bbac4602db5e04efae54392c60a99b" => :catalina
    sha256 "454cf0e4e8ad0721c52346d26d29b974e568fb0c3b9c12e60d1bc8f88ddc7bc1" => :mojave
    sha256 "23f7a5cbe93058e968781d35b1a94df1e23ff84942b1afb5ada3e33dfd5b9ca6" => :high_sierra
  end

  depends_on "pkg-config" => :build
  depends_on "freetype"
  depends_on "harfbuzz"
  depends_on "liblcf"
  depends_on "libpng"
  depends_on "libsndfile"
  depends_on "libvorbis"
  depends_on "libxmp"
  depends_on "mpg123"
  depends_on "pixman"
  depends_on "sdl2"
  depends_on "sdl2_mixer"
  depends_on "speexdsp"

  def install
    system "./configure", "--disable-dependency-tracking",
                          "--disable-silent-rules",
                          "--prefix=#{prefix}"
    system "make", "install"
  end

  test do
    assert_match /EasyRPG Player #{version}$/, shell_output("#{bin}/easyrpg-player -v")
  end
end
