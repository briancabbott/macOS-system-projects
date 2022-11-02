class Defaultbrowser < Formula
  desc "Command-line tool for getting & setting the default browser"
  homepage "https://github.com/kerma/defaultbrowser"
  url "https://github.com/kerma/defaultbrowser/archive/1.1.tar.gz"
  sha256 "56249f05da912bbe828153d775dc4f497f5a8b453210c2788d6a439418ac2ea3"
  license "MIT"

  bottle do
    cellar :any_skip_relocation
    sha256 "e796471951ee1290e11172aea1fff0b59c70cdbbfef43303bf11a3178e676a7f" => :big_sur
    sha256 "19e88d5316731019db1bafd3361af53fe3a4dd40348ac5a30bf34754b250f992" => :arm64_big_sur
    sha256 "e03bfa37fde424b0d7e76e6d2f99a26bad458e9d2bdf912db06d83d64bfe5a17" => :catalina
    sha256 "3a02a1fd0321f5070fa4ec9088a4a58a28ddb561bef5b94a0ccc31fdb896efa6" => :mojave
    sha256 "d07cc35e06f440584d698cde7ae5f27b09acfeafacc1499d331aa8553e945961" => :high_sierra
    sha256 "d0279f8c05c0c7828c534517897346fc231864ea8534f0ae04878ecfd51ca72d" => :sierra
    sha256 "f0ccf84abbd31469f80c4d232292dd280a978d3f04a1a6db46079902d9821d1e" => :el_capitan
  end

  def install
    system "make", "install", "PREFIX=#{prefix}"
  end

  test do
    # defaultbrowser outputs a list of browsers by default;
    # safari is pretty much guaranteed to be in that list
    assert_match "safari", shell_output("#{bin}/defaultbrowser")
  end
end
