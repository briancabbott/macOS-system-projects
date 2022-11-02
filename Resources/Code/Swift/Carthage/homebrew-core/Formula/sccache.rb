class Sccache < Formula
  desc "Used as a compiler wrapper and avoids compilation when possible"
  homepage "https://github.com/mozilla/sccache"
  url "https://github.com/mozilla/sccache/archive/v0.2.15.tar.gz"
  sha256 "7dbe71012f9b0b57d8475de6b36a9a3b4802e44a135e886f32c5ad1b0eb506e0"
  license "Apache-2.0"
  head "https://github.com/mozilla/sccache.git"

  bottle do
    cellar :any_skip_relocation
    sha256 "76080d09cb0b9bf50e7ef37609dc3e797b97b3c0f9deb4d71213b91524d67ab9" => :big_sur
    sha256 "e7674f3df1e319c5829551d64b4f2e25486ce7c68e2af84be9d13c496c296fbe" => :arm64_big_sur
    sha256 "d79d0f596f68b457b821a2d16444a53a93faa198049e4810b1a9016ef39fc7fe" => :catalina
    sha256 "76a1c87457acd3fbdd5f6352726911d2d0a524afce4639617f7559e80b6ae849" => :mojave
  end

  depends_on "rust" => :build
  depends_on "openssl@1.1"

  def install
    # Ensure that the `openssl` crate picks up the intended library.
    # https://crates.io/crates/openssl#manual-configuration
    ENV["OPENSSL_DIR"] = Formula["openssl@1.1"].opt_prefix

    system "cargo", "install", "--features", "all", *std_cargo_args
  end

  test do
    (testpath/"hello.c").write <<~EOS
      #include <stdio.h>
      int main() {
        puts("Hello, world!");
        return 0;
      }
    EOS
    system "#{bin}/sccache", "cc", "hello.c", "-o", "hello-c"
    assert_equal "Hello, world!", shell_output("./hello-c").chomp
  end
end
