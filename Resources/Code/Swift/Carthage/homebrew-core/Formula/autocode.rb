require "language/node"

class Autocode < Formula
  desc "Code automation for every language, library and framework"
  homepage "https://autocode.readme.io/"
  url "https://registry.npmjs.org/autocode/-/autocode-1.3.1.tgz"
  sha256 "952364766e645d4ddae30f9d6cc106fdb74d05afc4028066f75eeeb17c4b0247"
  license "Apache-2.0"

  livecheck do
    url :stable
  end

  bottle do
    cellar :any_skip_relocation
    rebuild 1
    sha256 "ddfc5b923a862daf2c1489d942e83f03dc99fdb3dcb2b7eebd67e92582174867" => :big_sur
    sha256 "780c41a7ada390dbefa61b56497f4cf53f9a5db0094b38c7064f9a4c21177fff" => :arm64_big_sur
    sha256 "451224479d19854f4f802b0ec63077080df91196917ad14d16e4a2308f247527" => :catalina
    sha256 "44742d0ccc3af3f27590445dbf2e89dffd8e684ff81521b5dc421449507879cd" => :mojave
    sha256 "a11f1fbbbf04052b9885a00abc88e7539a6c1992e35a62c6776df7ea32daf890" => :high_sierra
    sha256 "f369819b2f33327071a68455a14f66855286c7614977f06704f21c38e2df5f89" => :sierra
    sha256 "c321c73e1662332392c5949467c544e18db30849019555086ad14eeb097656d2" => :el_capitan
    sha256 "a0b7c969db9e2870e818587c7d832bbe0bb187cbc01346b85bb81a6097a9e015" => :yosemite
    sha256 "04effb5aecdd48e2a3c38435079424fd83f08dff206096f9807ff7c4ccd68b93" => :mavericks
  end

  depends_on "node"

  def install
    system "npm", "install", *Language::Node.std_npm_install_args(libexec)
    bin.install_symlink Dir["#{libexec}/bin/*"]
  end

  test do
    (testpath/".autocode/config.yml").write <<~EOS
      name: test
      version: 0.1.0
      description: test description
      author:
        name: Test User
        email: test@example.com
        url: https://example.com
      copyright: 2015 Test
    EOS
    system bin/"autocode", "build"
  end
end
