class Hy < Formula
  include Language::Python::Virtualenv

  desc "Dialect of Lisp that's embedded in Python"
  homepage "https://github.com/hylang/hy"
  url "https://files.pythonhosted.org/packages/e2/a8/d2118cf14aab7652d54283e6a9a199177f528610348e3712509a8596c0d0/hy-0.19.0.tar.gz"
  sha256 "3a5a1d76ddeb2f1d5de71ad1b1167799850db955b5eb0258e351fb182b2e6016"
  license "MIT"
  revision 1

  livecheck do
    url :stable
  end

  bottle do
    cellar :any_skip_relocation
    sha256 "15bce8a17bdf2b9331df03dee4bf33cc9f397bb76438e79f39bb543a580a6128" => :big_sur
    sha256 "12e1268704a5e05621258cdd082cf1a8d2118bd55a7f0edd7ed629df4b32a931" => :arm64_big_sur
    sha256 "8d26542c3b7206359d5a67555e30ea778d96be433a2bfc3539b4b593f50cb2d4" => :catalina
    sha256 "1a466ad15fbd8e08b20da924b51464674842074a1ff50722df4150675c122789" => :mojave
    sha256 "3517d09c61dac52cabd9dadead19f34280c5560e491fb2dfe46f47a55927d556" => :high_sierra
  end

  depends_on "python@3.9"

  resource "appdirs" do
    url "https://files.pythonhosted.org/packages/d7/d8/05696357e0311f5b5c316d7b95f46c669dd9c15aaeecbb48c7d0aeb88c40/appdirs-1.4.4.tar.gz"
    sha256 "7d5d0167b2b1ba821647616af46a749d1c653740dd0d2415100fe26e27afdf41"
  end

  resource "astor" do
    url "https://files.pythonhosted.org/packages/5a/21/75b771132fee241dfe601d39ade629548a9626d1d39f333fde31bc46febe/astor-0.8.1.tar.gz"
    sha256 "6a6effda93f4e1ce9f618779b2dd1d9d84f1e32812c23a29b3fff6fd7f63fa5e"
  end

  resource "colorama" do
    url "https://files.pythonhosted.org/packages/82/75/f2a4c0c94c85e2693c229142eb448840fba0f9230111faa889d1f541d12d/colorama-0.4.3.tar.gz"
    sha256 "e96da0d330793e2cb9485e9ddfd918d456036c7149416295932478192f4436a1"
  end

  resource "funcparserlib" do
    url "https://files.pythonhosted.org/packages/cb/f7/b4a59c3ccf67c0082546eaeb454da1a6610e924d2e7a2a21f337ecae7b40/funcparserlib-0.3.6.tar.gz"
    sha256 "b7992eac1a3eb97b3d91faa342bfda0729e990bd8a43774c1592c091e563c91d"
  end

  resource "rply" do
    url "https://files.pythonhosted.org/packages/71/04/e52242871e606389f232f07042747567fb354a91d9449cad7fa9febbe3b3/rply-0.7.7.tar.gz"
    sha256 "4d6d25703efd28fb3d5707f7b3bd4fe66c306159a5c25af10ba26d206a66d00d"
  end

  def install
    virtualenv_install_with_resources
  end

  test do
    (testpath/"test.hy").write "(print (+ 2 2))"
    assert_match "4", shell_output("#{bin}/hy test.hy")
    (testpath/"test.py").write shell_output("#{bin}/hy2py test.hy")
    assert_match "4", shell_output("#{Formula["python@3.9"].bin}/python3 test.py")
  end
end
