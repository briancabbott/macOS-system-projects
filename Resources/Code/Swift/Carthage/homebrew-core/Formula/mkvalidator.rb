class Mkvalidator < Formula
  desc "Tool to verify Matroska and WebM files for spec conformance"
  homepage "https://www.matroska.org/downloads/mkvalidator.html"
  url "https://downloads.sourceforge.net/project/matroska/mkvalidator/mkvalidator-0.5.2.tar.bz2"
  sha256 "2e2a91062f6bf6034e8049646897095b5fc7a1639787d5fe0fcef1f1215d873b"

  livecheck do
    url :stable
    regex(%r{url=.*?/mkvalidator[._-]v?(\d+(?:\.\d+)+)\.t}i)
  end

  bottle do
    cellar :any_skip_relocation
    sha256 "73efff490a07afdbccd396bcb5f0411d686757b5ede5d3c1340d76fd3b8d2a1f" => :big_sur
    sha256 "ee45e5e5abe82cd60c970947d680a93f6987ee879b0f504ebff40c150b0a58dd" => :catalina
    sha256 "d8ed0ae48b3922549518802148f3687a9bcab9f072624d619e077368a874e71b" => :mojave
    sha256 "5f0c85894cd7d4a7c5cdce1e26c5cc7c15ac7baa6c32a63e3474632f7727d8af" => :high_sierra
    sha256 "5f0c85894cd7d4a7c5cdce1e26c5cc7c15ac7baa6c32a63e3474632f7727d8af" => :sierra
    sha256 "6c253cdf3c824b6e37af7cca51bf05a930785286bc83ec367e10500d9645519c" => :el_capitan
  end

  resource "tests" do
    url "https://github.com/dunn/garbage/raw/c0e682836e5237eef42a000e7d00dcd4b6dcebdb/test.mka"
    sha256 "6d7cc62177ec3f88c908614ad54b86dde469dbd2b348761f6512d6fc655ec90c"
  end

  def install
    ENV.deparallelize # Otherwise there are races

    # Reported 2 Nov 2017 https://github.com/Matroska-Org/foundation-source/issues/31
    inreplace "configure", "\r", "\n"

    system "./configure"
    system "make", "-C", "mkvalidator"
    bindir = `corec/tools/coremake/system_output.sh`.chomp
    bin.install "release/#{bindir}/mkvalidator"
  end

  test do
    resource("tests").stage do
      system bin/"mkvalidator", "test.mka"
    end
  end
end
