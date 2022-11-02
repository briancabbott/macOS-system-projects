class Ii < Formula
  desc "Minimalist IRC client"
  homepage "https://tools.suckless.org/ii/"
  url "https://dl.suckless.org/tools/ii-1.8.tar.gz"
  sha256 "b9d9e1eae25e63071960e921af8b217ab1abe64210bd290994aca178a8dc68d2"
  license "MIT"
  head "https://git.suckless.org/ii", using: :git

  livecheck do
    url "https://dl.suckless.org/tools/"
    regex(/href=.*?ii[._-]v?(\d+(?:\.\d+)*)\.t/i)
  end

  bottle do
    cellar :any_skip_relocation
    rebuild 1
    sha256 "a58cf380ab73f600dadce4712328013a3ea3b0b65009fda1a3d2dc5e92741716" => :big_sur
    sha256 "a5de14cd12cba7a7ff58f2c5db87c300a3dcf69268dbbfbca552dbaf575568c0" => :arm64_big_sur
    sha256 "deb41b4c38d4b8e6cd2f3e9d8acea872a8daa694486ee59edd684248a7e74f4b" => :catalina
    sha256 "c8e535b535af9adf8c3c3e760849f581d3e93ec227ae9f0ae2f30490b44e9c4d" => :mojave
    sha256 "dcc9e7c86395491f5a62dd87dfcfb0f1b8b89a8f5ceb4e767ac70cf60ef350cd" => :high_sierra
    sha256 "a83511296e08d8ec1d126bb09574b02856f382f3f504b6f2b256cab6bd645ed1" => :sierra
    sha256 "eeba4fb4ec437895a9946bbbb00186ff05277ce9d57e8bbe29e1db5596d8a70f" => :el_capitan
  end

  def install
    # Fixed upstream, drop for next version
    inreplace "Makefile", "SRC = ii.c strlcpy.c", "SRC = ii.c"

    system "make", "install", "PREFIX=#{prefix}"
  end
end
