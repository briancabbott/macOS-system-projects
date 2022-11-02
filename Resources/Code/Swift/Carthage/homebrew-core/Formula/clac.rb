class Clac < Formula
  desc "Command-line, stack-based calculator with postfix notation"
  homepage "https://github.com/soveran/clac"
  url "https://github.com/soveran/clac/archive/0.3.3.tar.gz"
  sha256 "e751e31bd2d3cdf6daa80da0ea7761630767aa22df6954df35997d1fcc5fa8ae"
  license "BSD-2-Clause"

  bottle do
    cellar :any_skip_relocation
    sha256 "8582b92bdb32ca857fc3b8a5a33d0f19797edc1d1331ef79ffd9dc86ced96303" => :big_sur
    sha256 "5579baf3a429639b0cab2098d4dd5711fef715b6599a34b4b5ce11c383e8dbd6" => :arm64_big_sur
    sha256 "afa69f51c57695ae2e01a3ad9a44690fa746e925129ab2809550811cafe0fb3f" => :catalina
    sha256 "af0e4194b84f969e0a701f32c102dd64a432f6f675e6913716b81b96e548d451" => :mojave
    sha256 "5be5ed82ad0b8669aef16942e39e9d60341ab6201ee019b9b997ab5ee4ea76b1" => :high_sierra
  end

  def install
    system "make", "PREFIX=#{prefix}", "install"
  end

  test do
    assert_equal "7", shell_output("#{bin}/clac '3 4 +'").strip
  end
end
