class HelmAT2 < Formula
  desc "Kubernetes package manager"
  homepage "https://helm.sh/"
  url "https://github.com/helm/helm.git",
      tag:      "v2.17.0",
      revision: "a690bad98af45b015bd3da1a41f6218b1a451dbe"
  license "Apache-2.0"

  bottle do
    cellar :any_skip_relocation
    sha256 "432e81bffefbb026bd50058e920a424b1805b84efc634d78c93dfedb9fec3d5a" => :big_sur
    sha256 "831c4f5b7cf7fc1ab53364eeb2eeb6eff8babdbc51817b406b65a948ac6258c2" => :catalina
    sha256 "ab7ef44ce55c8b3597a2cb6dfe0ef93b74b389e6a4d6ab09c9a1ebe8dce5e594" => :mojave
    sha256 "a1c5cb86cce4fe2941c94309c8c75cd00ed9fae2e6edc6ea67aacadcf2f13c9e" => :high_sierra
  end

  keg_only :versioned_formula

  # See: https://helm.sh/blog/helm-v2-deprecation-timeline/
  deprecate! date: "2020-11-13", because: :deprecated_upstream

  depends_on "glide" => :build
  depends_on "go" => :build

  def install
    ENV["GOPATH"] = buildpath
    ENV["GLIDE_HOME"] = HOMEBREW_CACHE/"glide_home/#{name}"
    ENV.prepend_create_path "PATH", buildpath/"bin"
    ENV["TARGETS"] = "darwin/amd64"
    dir = buildpath/"src/k8s.io/helm"
    dir.install buildpath.children - [buildpath/".brew_home"]

    cd dir do
      system "make", "bootstrap"
      system "make", "build"

      bin.install "bin/helm"
      bin.install "bin/tiller"
      man1.install Dir["docs/man/man1/*"]

      output = Utils.safe_popen_read({ "SHELL" => "bash" }, bin/"helm", "completion", "bash")
      (bash_completion/"helm").write output

      output = Utils.safe_popen_read({ "SHELL" => "zsh" }, bin/"helm", "completion", "zsh")
      (zsh_completion/"_helm").write output

      prefix.install_metafiles
    end
  end

  test do
    system "#{bin}/helm", "create", "foo"
    assert File.directory? "#{testpath}/foo/charts"

    version_output = shell_output("#{bin}/helm version --client 2>&1")
    assert_match "GitTreeState:\"clean\"", version_output
    if build.stable?
      assert_match stable.instance_variable_get(:@resource).instance_variable_get(:@specs)[:revision], version_output
    end
  end
end
