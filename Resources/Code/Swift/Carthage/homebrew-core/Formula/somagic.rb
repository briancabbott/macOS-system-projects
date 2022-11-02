class Somagic < Formula
  desc "Linux capture program for the Somagic variants of EasyCAP"
  homepage "https://code.google.com/archive/p/easycap-somagic-linux/"
  url "https://storage.googleapis.com/google-code-archive-downloads/v2/code.google.com/easycap-somagic-linux/somagic-easycap_1.1.tar.gz"
  sha256 "3a9dd78a47335a6d041cd5465d28124612dad97939c56d7c10e000484d78a320"

  bottle do
    cellar :any
    sha256 "5451a0d35aa0aedcb43d1aeb6e080804333f8f7abaa090ceab62b1d02482389f" => :big_sur
    sha256 "5a8bb98812f68221bf3db7667aa281e9c18b111837bf1b5167adf30e498b53ff" => :arm64_big_sur
    sha256 "41d2479b3d2a267bbcd8c5db4ea7a8fe04c120d260d2ac9f087bd386012a3971" => :catalina
    sha256 "c2a69924be6f0d397b244955cfa841567ecd3171dc2674a4ac9748f49f58a44b" => :mojave
    sha256 "b6c11695d2c25a49a4a2c5795764a83615a214630bc25914e65fc691662617fc" => :high_sierra
    sha256 "377ecbdc01ebaab2acf1101aa00bbf5554e7d56b1b630baa28ef70d9deb10811" => :sierra
    sha256 "ed8a82423daaabaca0a7ab203edc68b3c0a1a1d617eb24d46486dfa974e9eb4f" => :el_capitan
    sha256 "9c87f9d7a694509b446ce726cedcb731c0185b589a1cdfa96c0346f883a75e5d" => :yosemite
    sha256 "0684417c6e1f1b498d10c5d24171217fb2e70ed0c8f9bacdd7365e8be8af20fc" => :mavericks
  end

  depends_on "libgcrypt"
  depends_on "libusb"
  depends_on "somagic-tools"

  def install
    system "make"
    system "make", "PREFIX=#{prefix}", "install"
  end

  def caveats
    <<~EOS
      Before running somagic-capture you must extract the official firmware from the CD.
      See https://code.google.com/archive/p/easycap-somagic-linux/wikis/GettingStarted.wiki for details.
    EOS
  end
end
