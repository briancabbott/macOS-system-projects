class Dsvpn < Formula
  desc "Dead Simple VPN"
  homepage "https://github.com/jedisct1/dsvpn"
  url "https://github.com/jedisct1/dsvpn/archive/0.1.4.tar.gz"
  sha256 "b98604e1ca2ffa7a909bf07ca7cf0597e3baa73c116fbd257f93a4249ac9c0c5"
  license "MIT"
  head "https://github.com/jedisct1/dsvpn.git"

  bottle do
    cellar :any_skip_relocation
    sha256 "9113686d8da5885ab67bae686914b0d27cc5eb2588a49b83f7c3fe66c89c039b" => :big_sur
    sha256 "5f0269b49b371b7330a7397c73ca1a8791da2e1215242eacd3e612f6705dd9c4" => :arm64_big_sur
    sha256 "a08464eca0167991c580594ecd9f1893a7be6d1cb522ceb385ff1883dca507c3" => :catalina
    sha256 "31a8359d756b673788aad04e1b776c0e1d5b6331f7e64494d3c6680280ea11ec" => :mojave
    sha256 "d34ff5d83b0b259c5051de2e2e8cf4599679d1d7e61dd282065afb0516fe62b1" => :high_sierra
  end

  def install
    sbin.mkpath
    system "make"
    system "make", "install", "PREFIX=#{prefix}"
  end

  def caveats
    <<~EOS
      dsvpn requires root privileges so you will need to run `sudo #{HOMEBREW_PREFIX}/sbin/dsvpn`.
      You should be certain that you trust any software you grant root privileges.
    EOS
  end

  test do
    expected = "tun device creation: Operation not permitted"
    assert_match expected, shell_output("#{sbin}/dsvpn client /dev/zero 127.0.0.1 0 2>&1", 1)
  end
end
