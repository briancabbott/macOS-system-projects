class Tag < Formula
  desc "Manipulate and query tags on macOS files"
  homepage "https://github.com/jdberry/tag/"
  url "https://github.com/jdberry/tag/archive/v0.10.tar.gz"
  sha256 "5ab057d3e3f0dbb5c3be3970ffd90f69af4cb6201c18c1cbaa23ef367e5b071e"
  license "MIT"
  revision 1
  head "https://github.com/jdberry/tag.git"

  bottle do
    cellar :any_skip_relocation
    sha256 "a63e067af22cda164f890108f610bfecd4bc3b2759fd1dd473dac59d1654a156" => :big_sur
    sha256 "68b99acc16647610b02c286aae5b302c7c2128164817d9eb197d2d5f9f51ca72" => :arm64_big_sur
    sha256 "e1572ae47d558d60983f7c1cbe9ae42a5c7f2dcb950762ab6c721e81351f5657" => :catalina
    sha256 "ee5dbe68476b6ae900b92486f3dc3c7a9755296c1fee54a75cd64c7d6af66763" => :mojave
    sha256 "5801c9fac7b1a4bad52f02fd8a09b64050ebc52515bd96115153c7049bd4619f" => :high_sierra
    sha256 "5711ce58bd5b224252f1869f84f937c6bca0775bf4c86a6a1168418c1218dc98" => :sierra
  end

  def install
    system "make", "install", "prefix=#{prefix}"
  end

  test do
    test_tag = "test_tag"
    test_file = Pathname.pwd+"test_file"
    touch test_file
    system "#{bin}/tag", "--add", test_tag, test_file
    assert_equal test_tag, `#{bin}/tag --list --no-name #{test_file}`.chomp
  end
end
