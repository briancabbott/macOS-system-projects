class Libpgm < Formula
  desc "Implements the PGM reliable multicast protocol"
  homepage "https://code.google.com/archive/p/openpgm/"
  url "https://storage.googleapis.com/google-code-archive-downloads/v2/code.google.com/openpgm/libpgm-5.2.122~dfsg.tar.gz"
  version "5.2.122"
  sha256 "e296f714d7057e3cdb87f4e29b1aecb3b201b9fcb60aa19ed4eec29524f08bd8"

  bottle do
    cellar :any
    rebuild 1
    sha256 "27381fca9259e51fafa0515c5b21a6642ebba34b6e55a0f78e5b9b39be7cd0ba" => :big_sur
    sha256 "f07813fb154e0e47acad079791155d2e5a9d69e45da24628b5052bdb0e2a971a" => :arm64_big_sur
    sha256 "416f7e3ff857e0c20f20c7c4774403059bbd540d003f0a0a546e122c603f7be6" => :catalina
    sha256 "0adcd6a17bbd37e11d0858c9ec7174b51932f33eb19a727c931acf1d719ab292" => :mojave
    sha256 "cccc90b754683842714480dc0a099abd303426ab2b47fd9fd8d0172717d9bc17" => :high_sierra
    sha256 "e84427aa937687e77701f8b0834866c86e6d4916685c769c4900403307b624c5" => :sierra
    sha256 "24765bd6efa0aa65a333e3d5bb5a48159875b81cae8ca99c479fbda4133f49b9" => :el_capitan
    sha256 "ae0d1d980f84677fcaa08b1d9f35f1c9d4858e4239598530b7485e9f248def73" => :yosemite
    sha256 "87ac77e422ffd9b72d1070c991064d0a8a9b5eb2d124f5cdd9911590b48bd291" => :mavericks
  end

  def install
    cd "openpgm/pgm" do
      system "./configure", "--disable-dependency-tracking",
                            "--prefix=#{prefix}"
      system "make", "install"
    end
  end
end
