class Qprint < Formula
  desc "Encoder and decoder for quoted-printable encoding"
  homepage "https://www.fourmilab.ch/webtools/qprint"
  url "https://www.fourmilab.ch/webtools/qprint/qprint-1.1.tar.gz"
  sha256 "ffa9ca1d51c871fb3b56a4bf0165418348cf080f01ff7e59cd04511b9665019c"

  bottle do
    cellar :any_skip_relocation
    sha256 "500367c9c89f50739d2b09f37f72ba1e0ec5418398d4570bf51363a725f57189" => :big_sur
    sha256 "05903a905caebf80944f4705898c5377849b7a411cf234614205b3136dba4a38" => :arm64_big_sur
    sha256 "081c0663cccb890326323fce7ac57b8bb020d3505eaf0d19f1824dd63c304de2" => :catalina
    sha256 "0915aa3e8b8481b717c4c84b0eda595821ecc99c9ffdcd0aa3e4952a3de9ae87" => :mojave
    sha256 "57950dba66674d62c84076374427f6c3de6d8cda81448c50b579c11b1b1959e4" => :high_sierra
    sha256 "f26387daf3d025dd45843784dd90fb3bf77609bdf0eb870f1b66782c89571950" => :sierra
    sha256 "9660443356a1f9571b39ea496349482e17f7c0d06829dd06945ca7680291c0bf" => :el_capitan
    sha256 "92470bcb0bd97c4d13181969ebeb0339faa85338ad139bf4a5ac19550635f5c1" => :yosemite
    sha256 "fbf2eadbf60b30557e3741e28545070bb377602aa8f1c1c49b5f65375381a2c4" => :mavericks
  end

  def install
    system "./configure", "--prefix=#{prefix}"
    system "make"
    bin.mkpath
    man1.mkpath
    system "make", "install"
  end

  test do
    msg = "test homebrew"
    encoded = pipe_output("#{bin}/qprint -e", msg)
    assert_equal msg, pipe_output("#{bin}/qprint -d", encoded)
  end
end
