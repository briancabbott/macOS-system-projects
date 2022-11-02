class SdlGfx < Formula
  desc "Graphics drawing primitives and other support functions"
  homepage "https://www.ferzkopp.net/joomla/content/view/19/14/"
  url "https://www.ferzkopp.net/Software/SDL_gfx-2.0/SDL_gfx-2.0.26.tar.gz"
  sha256 "7ceb4ffb6fc63ffba5f1290572db43d74386cd0781c123bc912da50d34945446"

  livecheck do
    url :homepage
    regex(/href=.*?SDL_gfx[._-]v?(\d+(?:\.\d+)+)\.t/i)
  end

  bottle do
    cellar :any
    sha256 "9c1c29ca5974afedf565fc0bddf2cb7b8f1d415905d728a60bbe09815e58efe5" => :big_sur
    sha256 "963c99a0ff8507b2efc3d0916fea2bf4656a3bd05ab00a4aee58298cad3e188c" => :arm64_big_sur
    sha256 "f06bf72be3f614ed944157f9e3fc0a13395ca4136eed4e1400762d791c576ad2" => :catalina
    sha256 "4a25e0639ae3c4e687bb8f9d6af00be3baf270565cd0402f7aa3af2a94e349d1" => :mojave
    sha256 "b1040e970fe68325a37c4a6af037206c28d12ae77f49851a0d28333e7c19a5e4" => :high_sierra
    sha256 "643210ccd7a2d9f2fc92d519900bbeb51c1f168729e40860c40e67629ce2ef8a" => :sierra
    sha256 "072983d26bc7e50acd12ef27adab047c3e14e45dff83e98be9ea005c7c107524" => :el_capitan
  end

  depends_on "sdl"

  def install
    extra_args = []
    extra_args << "--disable-mmx" if Hardware::CPU.arm?

    system "./configure", "--disable-dependency-tracking",
                          "--prefix=#{prefix}",
                          "--disable-sdltest",
                          *extra_args
    system "make", "install"
  end
end
