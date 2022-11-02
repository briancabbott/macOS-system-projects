class Sdl2Mixer < Formula
  desc "Sample multi-channel audio mixer library"
  homepage "https://www.libsdl.org/projects/SDL_mixer/"
  url "https://www.libsdl.org/projects/SDL_mixer/release/SDL2_mixer-2.0.4.tar.gz"
  sha256 "b4cf5a382c061cd75081cf246c2aa2f9df8db04bdda8dcdc6b6cca55bede2419"
  head "https://hg.libsdl.org/SDL_mixer", using: :hg

  livecheck do
    url :homepage
    regex(/SDL2_mixer[._-]v?(\d+(?:\.\d+)*)/i)
  end

  bottle do
    cellar :any
    sha256 "5369e47d093aa409b279f9f6acf2126d1dd1c7adf61438b0d0baa2d111c06af7" => :big_sur
    sha256 "96e7a8f9e28a01bea6cd404b64f9297370564ba4d6a5dee741f7337975b87209" => :arm64_big_sur
    sha256 "419d988dc795842301df16d2e57f7759417708b0d61466fea7ec1685db77bf1d" => :catalina
    sha256 "411aebe8a4b960a900879efc9d871575156efc174863beb135359679f3e7a8bf" => :mojave
    sha256 "af842a740632725bec40acd7418fa21aafcce0bee03d11a283c8c3509a235c78" => :high_sierra
    sha256 "359d8bd99a88d06f9484eb76b87b021ce48c777ac4583a0301ae0449e693cbf9" => :sierra
  end

  depends_on "pkg-config" => :build
  depends_on "libmodplug"
  depends_on "libvorbis"
  depends_on "sdl2"

  def install
    inreplace "SDL2_mixer.pc.in", "@prefix@", HOMEBREW_PREFIX

    args = %W[
      --prefix=#{prefix}
      --disable-dependency-tracking
      --disable-music-flac
      --disable-music-flac-shared
      --disable-music-midi-fluidsynth
      --disable-music-midi-fluidsynth-shared
      --disable-music-mod-mikmod-shared
      --disable-music-mod-modplug-shared
      --disable-music-mp3-mpg123
      --disable-music-mp3-mpg123-shared
      --disable-music-mp3-smpeg
      --disable-music-ogg-shared
      --enable-music-mod-mikmod
      --enable-music-mod-modplug
      --enable-music-ogg
    ]

    system "./configure", *args
    system "make", "install"
  end

  test do
    (testpath/"test.c").write <<~EOS
      #include <SDL2/SDL_mixer.h>

      int main()
      {
          int success = Mix_Init(0);
          Mix_Quit();
          return success;
      }
    EOS
    system ENV.cc, "-L#{lib}", "-lsdl2_mixer", "test.c", "-o", "test"
    system "./test"
  end
end
