class Goenv < Formula
  desc "Go version management"
  homepage "https://github.com/syndbg/goenv"
  url "https://github.com/syndbg/goenv/archive/1.23.3.tar.gz"
  sha256 "1559f2907ee0339328466fe93f3c9637b7674917db81754412c7f842749e3201"
  license "MIT"
  version_scheme 1
  head "https://github.com/syndbg/goenv.git"

  livecheck do
    url :head
    regex(/^v?(\d+(?:\.\d+)+)$/i)
  end

  bottle :unneeded

  def install
    inreplace "libexec/goenv", "/usr/local", HOMEBREW_PREFIX
    prefix.install Dir["*"]
    %w[goenv-install goenv-uninstall go-build].each do |cmd|
      bin.install_symlink "#{prefix}/plugins/go-build/bin/#{cmd}"
    end
  end

  test do
    assert_match "Usage: goenv <command> [<args>]", shell_output("#{bin}/goenv help")
  end
end
