class Cli53 < Formula
  desc "Command-line tool for Amazon Route 53"
  homepage "https://github.com/barnybug/cli53"
  url "https://github.com/barnybug/cli53/archive/0.8.17.tar.gz"
  sha256 "32b8e6ffe3234f87497328285c377b9280d1b302658e9acb45eb0dedbda0b14d"
  license "MIT"

  bottle do
    cellar :any_skip_relocation
    sha256 "0cb72c1182f1f323ce607fe9e016c3cf9352db57e18384da6b356164aab751cf" => :big_sur
    sha256 "1d7ce2cfe475ced9a092cf4c8c18d34feab5398249bc8672235c13004dfe9206" => :arm64_big_sur
    sha256 "65f9f13eb5c2f53afcb697f9699f8dd6b9df5f400908a2d3423879f8ca895942" => :catalina
    sha256 "73ec9efeaf423dd32530bd4a547b11522b54dd5666942735268bb7690479ff09" => :mojave
    sha256 "190e2e02b890cf099ca52d1468b224d16c82840a3a534db83a2aed5553b07bfc" => :high_sierra
  end

  depends_on "go" => :build

  def install
    system "go", "build", "-ldflags", "-s -w", "-trimpath", "-o", bin/"cli53", "./cmd/cli53"
    prefix.install_metafiles
  end

  test do
    assert_match "list domains", shell_output("#{bin}/cli53 help list")
  end
end
