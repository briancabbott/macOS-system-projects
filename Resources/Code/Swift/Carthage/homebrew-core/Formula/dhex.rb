class Dhex < Formula
  desc "Ncurses based advanced hex editor featuring diff mode and more"
  homepage "https://www.dettus.net/dhex/"
  url "https://www.dettus.net/dhex/dhex_0.69.tar.gz"
  sha256 "52730bcd1cf16bd4dae0de42531be9a4057535ec61ca38c0804eb8246ea6c41b"

  bottle do
    cellar :any_skip_relocation
    sha256 "f9737b2072e10b36cf34973fb1a18fbbdd570bbb4109656b89a51678220fd67e" => :big_sur
    sha256 "6b2818af033ee41f28f7718a9a310dc32b2b54272f7485934a643571e54b65b9" => :arm64_big_sur
    sha256 "d3f77d4f4b0f9899e74079753d754eb69296dfe5e44b6c2497c8680e0e941a23" => :catalina
    sha256 "2d6043ce881b7b407fa40691639c25c72d73a427fcdd39a56e0683f48b16b328" => :mojave
    sha256 "bfc2aa6c73ffff61a471b4dd8e183e493781a5c34baeddffc56fa89b0542a9bd" => :high_sierra
    sha256 "b83e63ad0f1e2910e1f2495903ac4077aa5caaabe8cb2702094f42c3921c7a9c" => :sierra
  end

  uses_from_macos "ncurses"

  def install
    inreplace "Makefile", "$(DESTDIR)/man", "$(DESTDIR)/share/man"
    bin.mkpath
    man1.mkpath
    man5.mkpath
    system "make", "install", "DESTDIR=#{prefix}"
  end

  test do
    assert_match("GNU GENERAL PUBLIC LICENSE",
                 pipe_output("#{bin}/dhex -g 2>&1", "", 0))
  end
end
