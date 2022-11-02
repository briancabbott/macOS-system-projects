class CoreosCt < Formula
  desc "Convert a Container Linux Config into Ignition"
  homepage "https://coreos.com/os/docs/latest/configuration.html"
  url "https://github.com/coreos/container-linux-config-transpiler/archive/v0.9.0.tar.gz"
  sha256 "140c2a5bfd2562a069882e66c4aee01290417f35ef0db06e11e74b2ccf52de7f"
  license "Apache-2.0"

  bottle do
    cellar :any_skip_relocation
    sha256 "a1b57820f00633b3e3af5d7cbee4accb970e8fee0e092fce3901801f3c84ce1e" => :big_sur
    sha256 "db582062a1743b1f01a0a90012e8a66d73b2518d88b4fb897afe9709565f6a95" => :catalina
    sha256 "883c46865e141d74fa7f0815fc51d3f9ea3145019f28dcec402fc457d3f67e27" => :mojave
    sha256 "5acd28f4e5dd0c74938cc7e7e10c5501badbab1d05316537a9afa173ff64f44e" => :high_sierra
    sha256 "8f09ba9875fe34e55de7fd25514493f41276d5c5e9f3cd37e00288fb6d44323e" => :sierra
    sha256 "9a48da5217b7e4b57e56702ee884fbc3067ccd895c2144cf7b02571cbcb80b42" => :el_capitan
  end

  deprecate! date: "2020-11-10", because: :repo_archived

  depends_on "go" => :build

  def install
    system "make", "all", "VERSION=v#{version}"
    bin.install "./bin/ct"
  end

  test do
    (testpath/"input").write <<~EOS
      passwd:
        users:
          - name: core
            ssh_authorized_keys:
              - ssh-rsa mykey
    EOS
    output = shell_output("#{bin}/ct -pretty -in-file #{testpath}/input").lines.map(&:strip).join
    assert_match /.*"sshAuthorizedKeys":\s*\["ssh-rsa mykey"\s*\].*/m, output.strip
  end
end
