class Eksctl < Formula
  desc "Simple command-line tool for creating clusters on Amazon EKS"
  homepage "https://eksctl.io"
  url "https://github.com/weaveworks/eksctl.git",
      tag:      "0.35.0",
      revision: "1eafbc7b3fa3d9c31575b51b8fbac718ec108051"
  license "Apache-2.0"
  revision 1
  head "https://github.com/weaveworks/eksctl.git"

  bottle do
    cellar :any_skip_relocation
    sha256 "665b157fc2746b7164dc5f124067342fa691ef66fad7981d94ca5c4bf7c2d1af" => :big_sur
    sha256 "fa29ae5e5a92f707a8d42c12f0c3a43c7072f997bbc4c31597417f019e749bc9" => :arm64_big_sur
    sha256 "ca88675d26cfc649026f1b087b4675bb40168affbc4b74d99634a43091158132" => :catalina
    sha256 "07fb2c3e42cd6297fd6dbaa45d75c5c92f5d0b4839f10a58675f3701bf732761" => :mojave
  end

  depends_on "counterfeiter" => :build
  depends_on "go" => :build
  depends_on "go-bindata" => :build
  depends_on "mockery" => :build
  depends_on "aws-iam-authenticator"

  # PR ref, https://github.com/weaveworks/eksctl/pull/2987
  # remove in next release
  patch do
    url "https://github.com/chenrui333/eksctl/commit/2207612.patch?full_index=1"
    sha256 "50870dd22647f6b6252678fb5302c81aa89ca04177f3f327a044d301eb483c14"
  end

  def install
    ENV["GOBIN"] = HOMEBREW_PREFIX/"bin"
    system "make", "build"
    bin.install "eksctl"

    # Install bash completion
    output = Utils.safe_popen_read("#{bin}/eksctl", "completion", "bash")
    (bash_completion/"eksctl").write output

    # Install zsh completion
    output = Utils.safe_popen_read("#{bin}/eksctl", "completion", "zsh")
    (zsh_completion/"_eksctl").write output
  end

  test do
    assert_match "The official CLI for Amazon EKS",
      shell_output("#{bin}/eksctl --help")

    assert_match "Error: couldn't create node group filter from command line options: --cluster must be set",
      shell_output("#{bin}/eksctl create nodegroup 2>&1", 1)
  end
end
