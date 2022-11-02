class Cdargs < Formula
  desc "Directory bookmarking system - Enhanced cd utilities"
  homepage "https://github.com/cbxbiker61/cdargs"
  url "https://github.com/cbxbiker61/cdargs/archive/2.1.tar.gz"
  sha256 "062515c3fbd28c68f9fa54ff6a44b81cf647469592444af0872b5ecd7444df7d"
  license "GPL-2.0"
  head "https://github.com/cbxbiker61/cdargs.git"

  bottle do
    cellar :any_skip_relocation
    sha256 "4c2ee17afed909adb4511fbbd7521e0cc4a852fd383f94735f1de76e63ffeeeb" => :big_sur
    sha256 "fb52b8d939ea7fde7c8579710b1bad8617e987214f2bfb730300b2e761ebf4dd" => :arm64_big_sur
    sha256 "0a40505138d5465211cc963f438683e38b88518b9f854e58b75d245e7a6fcd16" => :catalina
    sha256 "e78325dae8b29e9f7f5764537edf24d188be18ab27684392db9ebdbde1c9011b" => :mojave
    sha256 "fc93b68d48a7ae82eaf0816b6952bb1a6c7cc038c6439232cf01ea1b39bea3b0" => :high_sierra
  end

  depends_on "meson" => :build
  depends_on "ninja" => :build
  depends_on "pkg-config" => :build

  # fixes zsh usage using the patch provided at the cdargs homepage
  # (See https://www.skamphausen.de/cgi-bin/ska/CDargs)
  patch do
    url "https://raw.githubusercontent.com/Homebrew/formula-patches/85fa66a9/cdargs/1.35.patch"
    sha256 "adb4e73f6c5104432928cd7474a83901fe0f545f1910b51e4e81d67ecef80a96"
  end

  def install
    mkdir "build" do
      system "meson", *std_meson_args, ".."
      system "ninja", "-v"
      system "ninja", "install", "-v"
    end

    rm Dir["contrib/Makefile*"]
    prefix.install "contrib"
    bash_completion.install_symlink "#{prefix}/contrib/cdargs-bash.sh"
  end

  def caveats
    <<~EOS
      Support files for bash, tcsh, and emacs have been installed to:
        #{prefix}/contrib
    EOS
  end

  test do
    system "#{bin}/cdargs", "--version"
  end
end
