class Pelikan < Formula
  desc "Production-ready cache services"
  homepage "https://twitter.github.io/pelikan"
  url "https://github.com/twitter/pelikan/archive/0.1.2.tar.gz"
  sha256 "c105fdab8306f10c1dfa660b4e958ff6f381a5099eabcb15013ba42e4635f824"
  license "Apache-2.0"
  head "https://github.com/twitter/pelikan.git"

  bottle do
    cellar :any_skip_relocation
    sha256 "98b69e12d5ba1d3e8824e87f3fa5773a3bf7ba63dc2c32c73f07839b2c9d0e81" => :big_sur
    sha256 "22f695e695353e9317b34caf92789363464100d5ef63a7883a393767030e9951" => :arm64_big_sur
    sha256 "61441ad2aeeb6d14ab8fa6183944c1f4ab0733776e3f810ad17b80faf2f25faf" => :catalina
    sha256 "a313660eb003974995537cef07e391d3051218f7c65f3326c270b68f0855a59f" => :mojave
    sha256 "a80ae1b508d4eae75d03fc5ad07477039a50a37419681b2472af4f9dc5f240ea" => :high_sierra
    sha256 "37a675674b7ef33f07099029042f56c054f09b5d22400010d583fbfa41c0ce50" => :sierra
    sha256 "e314ce6288bf76e271bf69ce844e2e846b16cad68ce635faf1e5130c3c6911d0" => :el_capitan
    sha256 "ab04b8488e6272d0000c8e67842c4b286eb23459a6de9e9a392f14aa87c9978e" => :yosemite
    sha256 "80459134cbab7aa94ab55d38488b2058696f7408869306f75e80cfa0350ed40d" => :mavericks
  end

  depends_on "cmake" => :build

  def install
    mkdir "_build" do
      system "cmake", "..", *std_cmake_args
      system "make"
      system "make", "install"
    end
  end

  test do
    system "#{bin}/pelikan_twemcache", "-c"
  end
end
