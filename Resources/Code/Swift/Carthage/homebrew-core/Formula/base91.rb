class Base91 < Formula
  desc "Utility to encode and decode base91 files"
  homepage "https://base91.sourceforge.io"
  url "https://downloads.sourceforge.net/project/base91/basE91/0.6.0/base91-0.6.0.tar.gz"
  sha256 "02cfae7322c1f865ca6ce8f2e0bb8d38c8513e76aed67bf1c94eab1343c6c651"
  license "BSD-3-Clause"

  livecheck do
    url :stable
  end

  bottle do
    cellar :any_skip_relocation
    sha256 "039bcf75c09fb75a7472e8f92ef349f2908073b4ac76c1c4573d1a393e248229" => :big_sur
    sha256 "f97be9aed077e34c2aaf8aac0157fae476dedf2cf02eae3b23de8a74541e8135" => :arm64_big_sur
    sha256 "239a9f51f67e7b1de09d10c838dffd2c34ce0f6f58641269e2278d6478b36542" => :catalina
    sha256 "fca64b5013c75658646a7d758365a624aa5f3a89488573222f2bbb867b04cc49" => :mojave
    sha256 "3b9c972390a56bc2ea0be9943558018cc271802369b5b36ff0fa10391aaf1f57" => :high_sierra
    sha256 "7d43d307ad7fb92e10b21696e4f3d5880979f12b465db614f7ecaf9e4c9d4904" => :sierra
  end

  def install
    system "make"
    bin.install "base91"
    bin.install_symlink "base91" => "b91dec"
    bin.install_symlink "base91" => "b91enc"
    man1.install "base91.1"
    man1.install_symlink "base91.1" => "b91dec.1"
    man1.install_symlink "base91.1" => "b91enc.1"
  end

  test do
    assert_equal ">OwJh>Io2Tv!lE", pipe_output("#{bin}/b91enc", "Hello world")
    assert_equal "Hello world", pipe_output("#{bin}/b91dec", ">OwJh>Io2Tv!lE")
  end
end
