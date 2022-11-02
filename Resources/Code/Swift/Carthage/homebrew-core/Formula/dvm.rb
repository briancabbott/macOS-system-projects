class Dvm < Formula
  desc "Docker Version Manager"
  homepage "https://github.com/howtowhale/dvm"
  url "https://github.com/howtowhale/dvm/archive/1.0.2.tar.gz"
  sha256 "eb98d15c92762b36748a6f5fc94c0f795bf993340a4923be0eb907a8c17c6acc"
  license "Apache-2.0"
  revision 1

  bottle do
    cellar :any_skip_relocation
    sha256 "5f320e53c2734bed07fe70ac919232642d3a52d104bab787da9c08f251098942" => :big_sur
    sha256 "9c7cc18808affb5cc05958f3e501602c8d40889157c776dfb9f5ba9109a717b7" => :catalina
    sha256 "fa56fd369d0ef2dc43d29316d202f7cc3ca670765e07a3295429971929d93d24" => :mojave
    sha256 "d98c151704057dc821b67634c0387b15ed3b0e86b07e1eecd9c073f2f27abcd4" => :high_sierra
  end

  depends_on "go" => :build

  def install
    ENV["GOPATH"] = buildpath

    (buildpath/"src/github.com/howtowhale/dvm").install buildpath.children

    cd "src/github.com/howtowhale/dvm" do
      # Upstream release has a vendored dependency placed in the wrong path,
      # so adjust its location and relevant import statement.
      # Upstream acknowledged issue at https://github.com/howtowhale/dvm/issues/193
      mkdir "vendor/code.cloudfoundry.org"
      mv "vendor/github.com/pivotal-golang/archiver",
         "vendor/code.cloudfoundry.org/archiver"
      inreplace "dvm-helper/internal/downloader/downloader.go",
                "github.com/pivotal-golang/archiver/extractor",
                "code.cloudfoundry.org/archiver/extractor"

      system "make", "VERSION=#{version}", "UPGRADE_DISABLED=true"
      prefix.install "dvm.sh"
      bash_completion.install "bash_completion" => "dvm"
      (prefix/"dvm-helper").install "dvm-helper/dvm-helper"
    end
  end

  def caveats
    <<~EOS
      dvm is a shell function, and must be sourced before it can be used.
      Add the following command to your bash profile:
          [ -f #{opt_prefix}/dvm.sh ] && . #{opt_prefix}/dvm.sh
    EOS
  end

  test do
    output = shell_output("bash -c 'source #{prefix}/dvm.sh && dvm --version'")
    assert_match "Docker Version Manager version #{version}", output
  end
end
