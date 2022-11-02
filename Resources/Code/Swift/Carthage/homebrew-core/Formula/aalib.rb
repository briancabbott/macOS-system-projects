class Aalib < Formula
  desc "Portable ASCII art graphics library"
  homepage "https://aa-project.sourceforge.io/aalib/"
  url "https://downloads.sourceforge.net/project/aa-project/aa-lib/1.4rc5/aalib-1.4rc5.tar.gz"
  sha256 "fbddda9230cf6ee2a4f5706b4b11e2190ae45f5eda1f0409dc4f99b35e0a70ee"
  license "GPL-2.0-or-later"
  revision 1

  # The latest version in the formula is a release candidate, so we have to
  # allow matching of unstable versions.
  livecheck do
    url :stable
    regex(%r{url=.*?/aalib[._-]v?(\d+(?:\.\d+)+.*?)\.t}i)
  end

  bottle do
    cellar :any_skip_relocation
    sha256 "bfcc7abb45aaab7aa75c897d64a592bc868e3abc067be043d868f93d3fce113e" => :big_sur
    sha256 "ed1443a5da3a5695220d48ace4e35e559b7b53e2c527487b2caab5bd7f6c0ef2" => :arm64_big_sur
    sha256 "306da4a8cb39ef81eed8b81a7d1c1c7bd47405d8270d036dde9c07c630fa4ea3" => :catalina
    sha256 "a19ebbf86362d9a90900dd0b4013ebed778cd8681c0b3ed122c8bbaa04b11cbe" => :mojave
    sha256 "b2c5467ff9182645676381967b8dc89878f88900b19bed34ef432fd3257aa2a0" => :high_sierra
    sha256 "2c2d05720ca991422e4c27e3f770c29024c5197871cba67404f4e72a3cfaf002" => :sierra
    sha256 "9b3f19e5da28fb682aeb1fe40f1747d1b532490dd50262978aaefcb7afbc8804" => :el_capitan
    sha256 "9e08dd4e3545b05353f3158e4e756c20a301bef295b72183e1fd5fb1d6d8e897" => :yosemite
  end

  # Fix malloc/stdlib issue on macOS
  # Fix underquoted definition of AM_PATH_AALIB in aalib.m4
  patch do
    url "https://raw.githubusercontent.com/Homebrew/formula-patches/6e23dfb/aalib/1.4rc5.patch"
    sha256 "54aeff2adaea53902afc2660afb9534675b3ea522c767cbc24a5281080457b2c"
  end

  def install
    ENV.ncurses_define
    system "./configure", "--disable-debug",
                          "--disable-dependency-tracking",
                          "--prefix=#{prefix}",
                          "--mandir=#{man}",
                          "--infodir=#{info}",
                          "--enable-shared=yes",
                          "--enable-static=yes",
                          "--without-x"
    system "make", "install"
  end

  test do
    system "script", "-q", "/dev/null", bin/"aainfo"
  end
end
