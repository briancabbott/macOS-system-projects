class Wordgrinder < Formula
  desc "Unicode-aware word processor that runs in a terminal"
  homepage "https://cowlark.com/wordgrinder"
  url "https://github.com/davidgiven/wordgrinder/archive/0.8.tar.gz"
  sha256 "856cbed2b4ccd5127f61c4997a30e642d414247970f69932f25b4b5a81b18d3f"
  license "MIT"
  revision 1
  head "https://github.com/davidgiven/wordgrinder.git"

  bottle do
    cellar :any
    sha256 "d2cb8d569e0a7a02abae8deb32adf8a564042cfd6cddaeef4bc1dc16ab05e53b" => :big_sur
    sha256 "370093b3705f72a5d6b87bacd2e64e229f3d6ac82e52e92fe147c037d65f210b" => :arm64_big_sur
    sha256 "e084da6193fd984ac541e7c21044f80927b60b85ab69512d3824255be1c54d17" => :catalina
    sha256 "143c53429552e244089211458fc42bcdbb79171d5f98ae17db9c7175208c8ae4" => :mojave
  end

  depends_on "lua" => :build
  depends_on "ninja" => :build
  depends_on "pkg-config" => :build
  depends_on "ncurses"

  uses_from_macos "zlib"

  def install
    ENV["CURSES_PACKAGE"] = "ncursesw"
    system "make", "OBJDIR=#{buildpath}/wg-build"
    bin.install "bin/wordgrinder-builtin-curses-release" => "wordgrinder"
    man1.install "bin/wordgrinder.1"
    doc.install "README.wg"
  end

  test do
    system "#{bin}/wordgrinder", "--convert", "#{doc}/README.wg", "#{testpath}/converted.txt"
    assert_predicate testpath/"converted.txt", :exist?
  end
end
