class Quotatool < Formula
  desc "Edit disk quotas from the command-line"
  homepage "https://quotatool.ekenberg.se/"
  url "https://quotatool.ekenberg.se/quotatool-1.6.2.tar.gz"
  sha256 "e53adc480d54ae873d160dc0e88d78095f95d9131e528749fd982245513ea090"
  license "GPL-2.0"

  bottle do
    cellar :any_skip_relocation
    sha256 "e5dbc4f83caec774f6f05d65515c51bd8963c28415020698666d534030e91b23" => :big_sur
    sha256 "33cf581ce810cb4704669a05ee01b5cc963008f02393db65453ef06216ed257f" => :arm64_big_sur
    sha256 "b8787cedb9e50c5ea14b10fa2e6cf9ec948e7842a97d5e7212abef0ea87e26c6" => :catalina
    sha256 "2d2b6f53466ec7b211f44b0319966b7120e3bbf0e1d57c1f0ae3d272bc8f4ce4" => :mojave
    sha256 "bbf7543458972806f3c15b25bf7cd71276159b54ae1ada3beb12e6d29328ec0e" => :high_sierra
    sha256 "4d04c382c8cf8b0376b34ce12813be06e879fdf6b60711cf90643d08887304fb" => :sierra
    sha256 "da5c90f85204fa90a38da073765ec5c0f0a20333bcdcd131d79b682afa74233f" => :el_capitan
    sha256 "8af3549d42247f0b79458c96978f8f5e5fbe04cc1c0dd86f84accdf03e8e510f" => :yosemite
    sha256 "724a3fc561188de5e0e050f7459480cc8c613d399faee5290ec7a68b9715960d" => :mavericks
  end

  def install
    system "./configure", "--prefix=#{prefix}"
    sbin.mkpath
    man8.mkpath
    system "make", "install"
  end

  test do
    system "#{sbin}/quotatool", "-V"
  end
end
