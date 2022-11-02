class Seqtk < Formula
  desc "Toolkit for processing sequences in FASTA/Q formats"
  homepage "https://github.com/lh3/seqtk"
  url "https://github.com/lh3/seqtk/archive/v1.3.tar.gz"
  sha256 "5a1687d65690f2f7fa3f998d47c3c5037e792f17ce119dab52fff3cfdca1e563"
  license "MIT"

  bottle do
    cellar :any_skip_relocation
    sha256 "bdcc9b85644f98bbcdffbb84816e40714c254f8e9e8c8e5c042b4d25496e0010" => :big_sur
    sha256 "4e1d4c35dc64c5ea008ecb2e83bfc5d7a4047256e37923a981e5a6cd6038f368" => :arm64_big_sur
    sha256 "5abbf374f3dab69b198b98a3126f521b64baf01ac5ed69b99be91ffd97f891f8" => :catalina
    sha256 "b695a43103700d7d0d4a07d50d8effec280f7d7a781ff518a42dec2bef44801e" => :mojave
    sha256 "4f377caf93e5d334e739375a5dcf06782f1d85516988a26df3f8f53d172b1e6f" => :high_sierra
    sha256 "fd3ecced5ba8f5a9eab13f8f2184f6a69d08b58c1ef53ad6e74bb45cab9324f4" => :sierra
    sha256 "55541e7e9249ef15bd4423ad9a45903918c2b4b54f632bc0472fb24aee683701" => :el_capitan
  end

  uses_from_macos "zlib"

  def install
    system "make"
    bin.install "seqtk"
  end

  test do
    (testpath/"test.fasta").write <<~EOS
      >U00096.2:1-70
      AGCTTTTCATTCTGACTGCAACGGGCAATATGTCT
      CTGTGTGGATTAAAAAAAGAGTGTCTGATAGCAGC
    EOS
    assert_match "TCTCTG", shell_output("#{bin}/seqtk seq test.fasta")
  end
end
