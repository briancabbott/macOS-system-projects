class Prodigal < Formula
  desc "Microbial gene prediction"
  homepage "https://github.com/hyattpd/Prodigal"
  url "https://github.com/hyattpd/Prodigal/archive/v2.6.3.tar.gz"
  sha256 "89094ad4bff5a8a8732d899f31cec350f5a4c27bcbdd12663f87c9d1f0ec599f"
  license "GPL-3.0-or-later"

  livecheck do
    url :stable
    strategy :github_latest
  end

  bottle do
    cellar :any_skip_relocation
    sha256 "5f61811f05bc3e784428dd1ece760e6375f2624b103393e1809ece54659d440c" => :big_sur
    sha256 "abcf0f632ae6187b7b29e2ebd9680717c3678d3e2694b87840591a0f51d4db09" => :arm64_big_sur
    sha256 "5cebc25d98ba4439aa810c4e05c9f30e7ecf768035d135d0989cf58c18517a87" => :catalina
    sha256 "8751eedad40b08714b52a78b9cf48e4101ffa4b871a0ab943830a59137a67e53" => :mojave
    sha256 "c120fed8e29bb3b1a4ff69d5ca05e051a0fe3822784b3d585e142da3452d1ac1" => :high_sierra
    sha256 "a27fe5316181d4826e5aa5291d0fc1b1a7087c32c7b4e6aedabf1209d5a8ac36" => :sierra
    sha256 "70b432e3d3da1f4089680b06c0745b7dac3611f05d8ec9440faa918bc82d6fe5" => :el_capitan
  end

  # Prodigal will have incorrect output if compiled with certain compilers.
  # This will be fixed in the next release. Also see:
  # https://github.com/hyattpd/Prodigal/issues/34
  # https://github.com/hyattpd/Prodigal/issues/41
  # https://github.com/hyattpd/Prodigal/pull/35
  on_linux do
    patch do
      url "https://github.com/hyattpd/Prodigal/commit/cbbb5db21d120f100724b69d5212cf1275ab3759.patch?full_index=1"
      sha256 "fd292c0a98412a7f2ed06d86e0e3f96a9ad698f6772990321ad56985323b99a6"
    end
  end

  def install
    system "make", "install", "INSTALLDIR=#{bin}"
  end

  test do
    fasta = <<~EOS
      >U00096.2:1-70
      AGCTTTTCATTCTGACTGCAACGGGCAATATGTCTCTGTGTGGATTAAAAAAAGAGTGTCTGATAGCAGC
    EOS
    assert_match "CDS", pipe_output("#{bin}/prodigal -q -p meta", fasta, 0)
  end
end
