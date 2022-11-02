class Pex < Formula
  desc "Package manager for PostgreSQL"
  homepage "https://github.com/petere/pex"
  url "https://github.com/petere/pex/archive/1.20140409.tar.gz"
  sha256 "5047946a2f83e00de4096cd2c3b1546bc07be431d758f97764a36b32b8f0ae57"
  license "MIT"
  revision 3

  bottle do
    cellar :any_skip_relocation
    sha256 "02aca9cefc6e949f6a4f2854c70f31cc63fa1c88fb01e2f2e3108a3b608e0552" => :big_sur
    sha256 "2affe95fc76da2a2f6d289f2fb93a4ac16ace824fd1835d756d0664bc8e9ca25" => :arm64_big_sur
    sha256 "d266cf66e50d44748ed83ba2a20ffa0bd0530f637d98a85e3ab1b6eb11794319" => :catalina
    sha256 "427b8a701474aa879f8728ec463d3f20aad7c67f7b0ce330245015ec2830806a" => :mojave
    sha256 "ed1429f15df1e663735f27b1c7660e289953494b84a84bdd919a7eb077576a72" => :high_sierra
    sha256 "ed1429f15df1e663735f27b1c7660e289953494b84a84bdd919a7eb077576a72" => :sierra
    sha256 "ed1429f15df1e663735f27b1c7660e289953494b84a84bdd919a7eb077576a72" => :el_capitan
  end

  depends_on "postgresql"

  def install
    system "make", "install", "prefix=#{prefix}", "mandir=#{man}"
  end

  def caveats
    <<~EOS
      If installing for the first time, perform the following in order to setup the necessary directory structure:
        pex init
    EOS
  end

  test do
    assert_match "share/pex/packages", shell_output("#{bin}/pex --repo").strip
  end
end
