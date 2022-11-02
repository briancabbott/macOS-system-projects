class Shellharden < Formula
  desc "Bash syntax highlighter that encourages/fixes variables quoting"
  homepage "https://github.com/anordal/shellharden"
  url "https://github.com/anordal/shellharden/archive/v4.1.2.tar.gz"
  sha256 "8e5f623f9d58e08460d3ecabb28c53f1969bed09c2526f01b5e00362a8b08e7f"
  license "MPL-2.0"

  bottle do
    cellar :any_skip_relocation
    sha256 "954b7364a2db253c1fa0008f414dd8fa4831095ce1268512505e012a9bb1eaaf" => :big_sur
    sha256 "7b16335608271b3e49abfff5382c2c8c6013952c0e5f623e3525cfb8c94084fc" => :arm64_big_sur
    sha256 "85dd6255ff5f3410eafff6f5689bbcb7feac3f3f5d291139fc60597e2f46e5a5" => :catalina
    sha256 "fbe947b5d0596fb32bcbadddd904bf0b0d30c64053e00c13a41cc6ecf89d1e20" => :mojave
    sha256 "214a9dca5d9d013ada81c97e204c96815fd3376399c960d73973ffe4c7d653cf" => :high_sierra
  end

  depends_on "rust" => :build

  def install
    system "cargo", "install", *std_cargo_args
  end

  test do
    (testpath/"script.sh").write <<~EOS
      dog="poodle"
      echo $dog
    EOS
    system bin/"shellharden", "--replace", "script.sh"
    assert_match "echo \"$dog\"", (testpath/"script.sh").read
  end
end
