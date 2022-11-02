class Hyperfine < Formula
  desc "Command-line benchmarking tool"
  homepage "https://github.com/sharkdp/hyperfine"
  url "https://github.com/sharkdp/hyperfine/archive/v1.11.0.tar.gz"
  sha256 "740f4826f0933c693fb281e3542d312da9ccc8fd68cebe883359a8085ddd77e9"
  license "Apache-2.0"

  bottle do
    cellar :any_skip_relocation
    sha256 "011b82bcab88e4c6f9d3e45323d625dab76802baf2da59757ca76da6f92273c8" => :big_sur
    sha256 "8bc60ab84503e4369129988d410083e3fdf0653ee68f650ea8a64d0d7abe3edd" => :arm64_big_sur
    sha256 "f00c3c13082cb07e6c40bace87ab0e1a03572d1296661d6d217fefc525f2075e" => :catalina
    sha256 "162a1fac5ab92895d620f85a2a9de1e77786ba3a5a02037cfee57a9d50048c72" => :mojave
    sha256 "588d7e8466b0fa4303eda838677bb68fe888521bbce1ce89dbf6f28b304ffbc6" => :high_sierra
  end

  depends_on "rust" => :build

  def install
    ENV["SHELL_COMPLETIONS_DIR"] = buildpath
    system "cargo", "install", *std_cargo_args
    man1.install "doc/hyperfine.1"
    bash_completion.install "hyperfine.bash"
    fish_completion.install "hyperfine.fish"
    zsh_completion.install "_hyperfine"
  end

  test do
    output = shell_output("#{bin}/hyperfine 'sleep 0.3'")
    assert_match "Benchmark #1: sleep", output
  end
end
