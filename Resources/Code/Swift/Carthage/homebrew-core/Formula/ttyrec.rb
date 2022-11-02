class Ttyrec < Formula
  desc "Terminal interaction recorder and player"
  homepage "http://0xcc.net/ttyrec/"
  url "http://0xcc.net/ttyrec/ttyrec-1.0.8.tar.gz"
  sha256 "ef5e9bf276b65bb831f9c2554cd8784bd5b4ee65353808f82b7e2aef851587ec"
  revision 1

  livecheck do
    url :homepage
    regex(/href=["']?ttyrec[._-]v?(\d+(?:\.\d+)+)\.t/i)
  end

  bottle do
    cellar :any_skip_relocation
    sha256 "dc7756b323c5faf2006093ac2873d7805f5ddfc06df6bf5bcbcdd4fa70b2c328" => :big_sur
    sha256 "5538f4c65b9395dd35a7d9f975a7da59a9e9b1bc4cf09725b86d61c48755306a" => :arm64_big_sur
    sha256 "6d893647087afa85234f60103507a5a878360d018816c557534d469c4edf7bf9" => :catalina
    sha256 "fa4e19544555ebf7956beceaa656bb8aed894f26b82683a5db32b88501cc5a85" => :mojave
    sha256 "8121debd07c4ecdd24d86fc7dadb00a7807e028f512418b5ba0d85768619628d" => :high_sierra
    sha256 "0323b20a0905ad1c3a2f997714572d779bcf6db63d8798840c14f6a75fd70cd5" => :sierra
    sha256 "ec05f403a1aa20da2e1fbd6f4d912b3d31fa1fd100c9adba68c928146a50bbc0" => :el_capitan
  end

  resource "matrix.tty" do
    url "http://0xcc.net/tty/tty/matrix.tty"
    sha256 "76b8153476565c5c548aa04c2eeaa7c7ec8c1385bcf8b511c68915a3a126fdeb"
  end

  def install
    # macOS has openpty() in <util.h>
    # Reported by email to satoru@0xcc.net on 2017-12-20
    inreplace "ttyrec.c", "<libutil.h>", "<util.h>"

    system "make", "CFLAGS=#{ENV.cflags} -DHAVE_openpty"
    bin.install %w[ttytime ttyplay ttyrec]
    man1.install Dir["*.1"]
  end

  test do
    resource("matrix.tty").stage do
      assert_equal "9\tmatrix.tty", shell_output("#{bin}/ttytime matrix.tty").strip
    end
  end
end
