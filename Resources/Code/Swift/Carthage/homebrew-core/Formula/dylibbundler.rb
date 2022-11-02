class Dylibbundler < Formula
  desc "Utility to bundle libraries into executables for macOS"
  homepage "https://github.com/auriamg/macdylibbundler"
  url "https://github.com/auriamg/macdylibbundler/archive/0.4.5-release.tar.gz"
  sha256 "cd41e45115371721e0aa94e70c457134acf49f6d5f6d359b5bae060fd876d887"
  license "MIT"
  head "https://github.com/auriamg/macdylibbundler.git"

  bottle do
    cellar :any_skip_relocation
    sha256 "2c4d2e3d71590903992d5735f32189c532533dffb3d1874f08932afe33c0e5b8" => :big_sur
    sha256 "475b571e44abd0e57c924e004b94dba1fd1600b11fb6e7094afdab42406865d8" => :arm64_big_sur
    sha256 "0794eea61309318e5aa8686a5781cbd5c534b1f9b481d38502a7343007cfe77e" => :catalina
    sha256 "f2554553b0c00165394e41ade50712f490331d7bf084792abc2cb4f12ae1164e" => :mojave
    sha256 "60b4e47bfbb3450f6901e6c104d37530940e9cc22abacaacbe37eb4539b820c6" => :high_sierra
    sha256 "c8f470a6e3c0c5eaf632dd384f5098f0e59f60ab2c873482424f7c6729a4fe07" => :sierra
  end

  def install
    system "make"
    bin.install "dylibbundler"
  end

  test do
    system "#{bin}/dylibbundler", "-h"
  end
end
