class Gx < Formula
  desc "Language-agnostic, universal package manager"
  homepage "https://github.com/whyrusleeping/gx"
  url "https://github.com/whyrusleeping/gx/archive/v0.14.3.tar.gz"
  sha256 "2c0b90ddfd3152863f815c35b37e94d027216c6ba1c6653a94b722bf6e2b015d"
  license "MIT"
  head "https://github.com/whyrusleeping/gx.git"

  bottle do
    cellar :any_skip_relocation
    sha256 "5171470b8fe21652a1ae5fef81f0b463a3c10fdac821134f2e8d7635af1ccdda" => :big_sur
    sha256 "7099419ae5d6da42d66de65b1a4b9355f586f7484a6a738d1caa1c77ff917670" => :arm64_big_sur
    sha256 "f737f5829c0e1ce2ff58c56515e77f3797c30d614a53ebbf663985d5564c62db" => :catalina
    sha256 "bd03f428c3e52561caefaa09c6abc92b21faa226ad02abeeb6c74217ca1dfbbf" => :mojave
    sha256 "e6634c1b68abcb48984a7d681248393ca26824a81496c567ef23029ff9a892f5" => :high_sierra
  end

  depends_on "go" => :build

  def install
    system "go", "build", "-o", bin/"gx"
  end

  test do
    assert_match "ERROR: no package found in this directory or any above", shell_output("#{bin}/gx deps", 1)
  end
end
