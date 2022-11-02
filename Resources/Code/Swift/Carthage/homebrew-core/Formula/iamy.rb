class Iamy < Formula
  desc "AWS IAM import and export tool"
  homepage "https://github.com/99designs/iamy"
  url "https://github.com/99designs/iamy/archive/v2.3.2.tar.gz"
  sha256 "66d44dd6af485b2b003b0aa1c8dcd799f7bae934f1ce1efb7e5d5f6cfe7f8bf2"
  license "MIT"
  head "https://github.com/99designs/iamy.git"

  bottle do
    cellar :any_skip_relocation
    rebuild 1
    sha256 "02aa2ecaab3e449d3ca641e88a22dca829a969d6984190146514b807cdf2b3a2" => :big_sur
    sha256 "9ebdffb035a4c24087c7f3a7f9918ab1734fd00c6baa64da4e49fc721d0ea553" => :arm64_big_sur
    sha256 "f5b5f5a4db400dc6021f206a23e90fbff7666f92cfdef296efe86b2db3ba9aa4" => :catalina
    sha256 "3f01263009a39e769d8144ba9c284d95421ab919c78f549f0e36cbf56985693c" => :mojave
  end

  depends_on "go" => :build
  depends_on "awscli"

  def install
    system "go", "build", *std_go_args, "-ldflags",
            "-s -w -X main.Version=v#{version}"
  end

  test do
    ENV.delete "AWS_ACCESS_KEY"
    ENV.delete "AWS_SECRET_KEY"
    output = shell_output("#{bin}/iamy pull 2>&1", 1)
    assert_match "Can't determine the AWS account", output
  end
end
