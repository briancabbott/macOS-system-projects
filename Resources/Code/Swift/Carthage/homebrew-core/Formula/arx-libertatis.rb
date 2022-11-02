class ArxLibertatis < Formula
  desc "Cross-platform, open source port of Arx Fatalis"
  homepage "https://arx-libertatis.org/"
  license "GPL-3.0"
  revision 2

  stable do
    url "https://arx-libertatis.org/files/arx-libertatis-1.1.2.tar.xz"
    sha256 "82adb440a9c86673e74b84abd480cae968e1296d625b6d40c69ca35b35ed4e42"

    # Add a missing include to CMakeLists.txt
    patch do
      url "https://github.com/arx/ArxLibertatis/commit/442ba4af978160abd3856a9daec38f5b6e213cb4.patch?full_index=1"
      sha256 "de361866cc51c14f317a67dcfd3b736160a577238f931c78a525ea2864b1add9"
    end
  end

  livecheck do
    url :head
    regex(/^v?(\d+(?:\.\d+)+)$/i)
  end

  bottle do
    cellar :any
    sha256 "2a9e06b2b91e1133389728b2ad0c81f23d95a266a451144fb8639953b5a96cd3" => :big_sur
    sha256 "204111aff418fd39e00ab1fcb961f2c7d5a7580223ecdbac4011b7c1f3122f03" => :arm64_big_sur
    sha256 "b93ffc0870dffd0bab99117814e3c094fc019c2315bdd8fc35f687c1009dd661" => :catalina
    sha256 "39fc49249e5a82bd067c05bcd056b454a90ace91f364b3c33534901827247b2c" => :mojave
    sha256 "2fe2043845655c6f3e75be1dc7213826fd142f806fd7b59006fdef940584e92a" => :high_sierra
  end

  head do
    url "https://github.com/arx/ArxLibertatis.git"

    resource "arx-libertatis-data" do
      url "https://github.com/arx/ArxLibertatisData.git"
    end
  end

  depends_on "boost" => :build
  depends_on "cmake" => :build
  depends_on "glm" => :build
  depends_on "freetype"
  depends_on "glew"
  depends_on "innoextract"
  depends_on "sdl"

  conflicts_with "rnv", because: "both install `arx` binaries"

  def install
    args = std_cmake_args

    # The patches for these aren't straightforward to backport because of
    # other changes; these minimal inreplaces get it building.
    # HEAD is fine, and the next stable release will contain these changes.
    if build.stable?
      # https://github.com/arx/ArxLibertatis/commit/39fb9a0e3a6888a6a5f040e39896e88750c89065
      inreplace "src/platform/Time.cpp", "clock_t ", "clockid_t "

      # Version parsing is broken in the current stable; fixed upstream.
      # This hardcodes the current version based on data from VERSION.
      inreplace "src/core/Version.cpp.in" do |s|
        s.gsub! "${VERSION_COUNT}", "5"
        s.gsub! "${VERSION_2}", "10"
        s.gsub! "${VERSION_0}", "1.1.2"
        s.gsub! "${GIT_SUFFIX_5}", "+Homebrew-1"
        s.gsub! "${VERSION_4}", "Rhaa Movis"
      end
    end

    # Install prebuilt icons to avoid inkscape and imagemagick deps
    if build.head?
      (buildpath/"arx-libertatis-data").install resource("arx-libertatis-data")
      args << "-DDATA_FILES=#{buildpath}/arx-libertatis-data"
    end

    mkdir "build" do
      system "cmake", "..", *args
      system "make", "install"
    end
  end

  def caveats
    <<~EOS
      This package only contains the Arx Libertatis binary, not the game data.
      To play Arx Fatalis you will need to obtain the game from GOG.com and
      install the game data with:

        arx-install-data /path/to/setup_arx_fatalis.exe
    EOS
  end

  test do
    system "#{bin}/arx", "-h"
  end
end
