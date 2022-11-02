class Lizard < Formula
  desc "Efficient compressor with very fast decompression"
  homepage "https://github.com/inikep/lizard"
  url "https://github.com/inikep/lizard/archive/v1.0.tar.gz"
  sha256 "6f666ed699fc15dc7fdaabfaa55787b40ac251681b50c0d8df017c671a9457e6"
  license all_of: ["BSD-2-Clause", "GPL-2.0-or-later"]
  version_scheme 1

  livecheck do
    url :stable
    strategy :github_latest
  end

  bottle do
    cellar :any
    rebuild 1
    sha256 "3ddb2ae111832e46648ef4b0bf73bb890f96afd6cdb5dedc847857162163079e" => :big_sur
    sha256 "25adf9383bbad3ab6c4f51e38ea46ebe4fc636cc347c8625b2fbd65e89a3144d" => :arm64_big_sur
    sha256 "18fe5004080acea3a2799a0e1bce34e0382bea9528a1ec036267c1eb8a702e3b" => :catalina
    sha256 "7375bcd75ec034939751ee0f44dd703ac81431957a92712d26fec1682e00ebc7" => :mojave
    sha256 "a42e90e02b4074e0c864ae32fe5833977cebd50b8f9c74339c7a91dcf169b098" => :high_sierra
  end

  def install
    system "make", "PREFIX=#{prefix}", "install"
    cd "examples" do
      system "make"
      (pkgshare/"tests").install "ringBufferHC", "ringBuffer", "lineCompress", "doubleBuffer"
    end
  end

  test do
    (testpath/"tests/test.txt").write <<~EOS
      Homebrew is a free and open-source software package management system that simplifies the installation
      of software on Apple's macOS operating system and Linux. The name means building software on your Mac
      depending on taste. Originally written by Max Howell, the package manager has gained popularity in the
      Ruby on Rails community and earned praise for its extensibility. Homebrew has been recommended for its
      ease of use as well as its integration into the command line. Homebrew is a non-profit project member
      of the Software Freedom Conservancy, and is run entirely by unpaid volunteers.
    EOS

    cp_r pkgshare/"tests", testpath
    cd "tests" do
      system "./ringBufferHC", "./test.txt"
      system "./ringBuffer", "./test.txt"
      system "./lineCompress", "./test.txt"
      system "./doubleBuffer", "./test.txt"
    end
  end
end
