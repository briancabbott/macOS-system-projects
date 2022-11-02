class Mpck < Formula
  desc "Check MP3 files for errors"
  homepage "https://checkmate.gissen.nl/"
  url "https://checkmate.gissen.nl/checkmate-0.21.tar.gz"
  sha256 "a27b4843ec06b069a46363836efda3e56e1daaf193a73a4da875e77f0945dd7a"
  license "GPL-2.0"

  bottle do
    cellar :any_skip_relocation
    sha256 "215f2f66b6567409359c6a0f784702df9fcc2e0c86edcab52fc40f91b6911bb9" => :big_sur
    sha256 "f963c58102f58169a5ea1d6264f3ea1093a62fd6461332d5e70d0e1ad9aa5d79" => :arm64_big_sur
    sha256 "45f8695f2758dd07237c333e8a17aa38f8d0aed4e87e8b5dc7fea7bf4537b0e9" => :catalina
    sha256 "e819ac8ce7eab3b4f83bcdf83cfbb129a9e3cebb36e314dabca646f808ed6257" => :mojave
    sha256 "3ecd47f83f5645cfaf2bfef23b5b9a1b14bb36f2ec146409ca44d9d5f25c3401" => :high_sierra
    sha256 "cd283270b83cf83c3e3a3c393404c1eca16e1620ced195821b97fe5ad6b39236" => :sierra
  end

  def install
    system "./configure", "--disable-dependency-tracking",
                          "--prefix=#{prefix}"
    system "make", "install"
  end

  test do
    system "#{bin}/mpck", test_fixtures("test.mp3")
  end
end
