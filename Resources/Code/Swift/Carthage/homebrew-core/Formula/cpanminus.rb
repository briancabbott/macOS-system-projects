class Cpanminus < Formula
  desc "Get, unpack, build, and install modules from CPAN"
  homepage "https://github.com/miyagawa/cpanminus"
  url "https://github.com/miyagawa/cpanminus/archive/1.9019.tar.gz"
  sha256 "d0a37547a3c4b6dbd3806e194cd6cf4632158ebed44d740ac023e0739538fb46"
  # dual licensed same as perl (GPL-1.0 or Artistic-1.0)
  license "GPL-1.0"
  head "https://github.com/miyagawa/cpanminus.git"

  livecheck do
    url :head
    regex(/^v?(\d+(?:\.\d+)+)$/i)
  end

  bottle :unneeded

  def install
    cd "App-cpanminus" do
      bin.install "cpanm"
    end
  end

  test do
    system "#{bin}/cpanm", "Test::More"
  end
end
