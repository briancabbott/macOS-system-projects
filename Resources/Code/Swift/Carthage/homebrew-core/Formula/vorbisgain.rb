class Vorbisgain < Formula
  desc "Add Replay Gain volume tags to Ogg Vorbis files"
  homepage "https://sjeng.org/vorbisgain.html"
  url "https://sjeng.org/ftp/vorbis/vorbisgain-0.37.tar.gz"
  sha256 "dd6db051cad972bcac25d47b4a9e40e217bb548a1f16328eddbb4e66613530ec"
  license "LGPL-2.1"

  bottle do
    cellar :any
    sha256 "c70e02a12f7c232b752afebcee063ecb031eb120f64c58bb6a95325100aa6767" => :big_sur
    sha256 "5763a2b6c8f9c2d5c27e708a8cbc88f5274dcb7e44f5f5ecf2b29b909e18e017" => :arm64_big_sur
    sha256 "b49eb04725bc5ab78fa94e261bb23598b4b4cacf1e8508a9ea543b0069947894" => :catalina
    sha256 "75aa0134fbe5f63549d0e9328f6ae2d3513670e74d5f35ca7c633917a9383aee" => :mojave
    sha256 "86d9a629d242005f13a1de8a591aee61455059b9072ab528afbebe63ecda9613" => :high_sierra
    sha256 "3572efe1e0741ee5b1f3cc0499325ae59acec5f9ab44df5987bab22312d9e3af" => :sierra
    sha256 "00f7047e5d884dbf22ed036154961b41d4ad6ae8295c55043929b008ae82a9f7" => :el_capitan
    sha256 "a1315eaaaf667f7486deb2b899c422e3cc9caa8f9e771221dca12b0ecc085dc7" => :yosemite
    sha256 "7bff88a3512ff2d28846b89b89f1cfd130e5934f1f6301d36e66ccc26b896281" => :mavericks
  end

  depends_on "libogg"
  depends_on "libvorbis"

  def install
    system "./configure", "--prefix=#{prefix}", "--mandir=#{man}"
    system "make", "install"
  end

  test do
    system "#{bin}/vorbisgain", "--version"
  end
end
