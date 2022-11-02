class TmuxMemCpuLoad < Formula
  desc "CPU, RAM memory, and load monitor for use with tmux"
  homepage "https://github.com/thewtex/tmux-mem-cpu-load"
  url "https://github.com/thewtex/tmux-mem-cpu-load/archive/v3.4.0.tar.gz"
  sha256 "a773994e160812a964abc7fc4e8ec16b7d9833edb0a66e5c67f287c7c5949ecb"
  license "Apache-2.0"
  head "https://github.com/thewtex/tmux-mem-cpu-load.git"

  bottle do
    cellar :any_skip_relocation
    sha256 "4c40768aa4599ae044cace8455205f9503441c64adaabb8f5c4c9fc221f89b27" => :big_sur
    sha256 "ebad3961141c5ef92cda90430a594587454b06464e4b386e7f5dae7158a18e97" => :arm64_big_sur
    sha256 "5006666230be68b50c097cdb4ce12e20c37ae565cb1de9163861918d42910834" => :catalina
    sha256 "6da11cf3e7664d4b75de9a276c9b3823072a9d46855e2aaa2caeaa57ffdb9221" => :mojave
    sha256 "ac291740dbf05c7cae025836caf5c2ad1f375f9060fc871dfc5adf51abe2a4c2" => :high_sierra
    sha256 "8743cb844ff2a55657f2f1eb7bfae300c02a3fdf255fdd5e8242d1a60103838d" => :sierra
    sha256 "9e2c7e5fd03feb98cead3f366a9cc35375cee80c30fd570c742440d69319c296" => :el_capitan
    sha256 "abd6293238671268ea1f0362518cd82c4b3133cb42b0327d579c93768ea81110" => :yosemite
    sha256 "24e52a177d0201edf30621a648c7cbbf1f2cc7e4bd9f9145a7f8c258d9219725" => :mavericks
  end

  depends_on "cmake" => :build

  def install
    ENV.cxx11
    system "cmake", ".", *std_cmake_args
    system "make", "install"
  end

  test do
    system bin/"tmux-mem-cpu-load"
  end
end
