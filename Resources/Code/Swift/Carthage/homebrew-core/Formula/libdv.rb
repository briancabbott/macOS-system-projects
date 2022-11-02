class Libdv < Formula
  desc "Codec for DV video encoding format"
  homepage "https://libdv.sourceforge.io"
  url "https://downloads.sourceforge.net/project/libdv/libdv/1.0.0/libdv-1.0.0.tar.gz"
  sha256 "a305734033a9c25541a59e8dd1c254409953269ea7c710c39e540bd8853389ba"

  livecheck do
    url :stable
  end

  bottle do
    cellar :any
    rebuild 1
    sha256 "81db616fc05c65d944af1a500e9d647764e361419040b0007d9efc85ebfe3d31" => :big_sur
    sha256 "a72d9919c11d6950fcd115e6fa0e6cbac86ec6f06d8ade46b642006f652bf53f" => :arm64_big_sur
    sha256 "ee4a56e9a1462b87632dca0048808d4918b5a72fd943cce8ee600d138c9f67f6" => :catalina
    sha256 "ec2564e27b321ebb35a3278d6e7411d17d1b1acad5dbcd40106cbcaf24205b53" => :mojave
    sha256 "0f7c7db1baa95682ad66b9d628d51978f162558f6d8296715a38150f83a7c72f" => :high_sierra
    sha256 "9ea1a006d7aa954c5a1d61497f9f7f43e0b1bd5bce911b6d334a693d8af58671" => :sierra
    sha256 "0624e82748111d0a8a050a802ec4251c443127c39c93b3b2469a00816a602040" => :el_capitan
    sha256 "49262f766082fa4c9a509236bdaf5eec8746a8c4b9724fc83521bfc5725660c7" => :yosemite
    sha256 "830002340f10dae43ba81d370bfbcd92a40f8b9330ed03fcb627b70983069a7b" => :mavericks
  end

  depends_on "popt"

  def install
    # This fixes an undefined symbol error on compile.
    # See the port file for libdv:
    #   https://trac.macports.org/browser/trunk/dports/multimedia/libdv/Portfile
    # This flag is the preferred method over what macports uses.
    # See the apple docs: https://cl.ly/2HeF bottom of the "Finding Imported Symbols" section
    ENV.append "LDFLAGS", "-undefined dynamic_lookup"

    system "./configure", "--disable-dependency-tracking",
                          "--prefix=#{prefix}",
                          "--disable-gtktest",
                          "--disable-gtk",
                          "--disable-asm",
                          "--disable-sdltest"
    system "make", "install"
  end
end
