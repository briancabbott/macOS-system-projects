class Richmd < Formula
  include Language::Python::Virtualenv

  desc "Format Markdown in the terminal with Rich"
  homepage "https://github.com/willmcgugan/rich"
  url "https://files.pythonhosted.org/packages/c7/dd/634c0474026b323cddd9b38939e40b502d04eebd36b47d994d4919de5d4c/rich-9.8.1.tar.gz"
  sha256 "0ec853f882613e75a5e46d545ddaa48cad235c616eaeb094792012fe22e8b2c6"
  license "MIT"

  bottle do
    cellar :any_skip_relocation
    sha256 "0007b69601ace0d8cbdad0d0e86766bcaf347b7a75d1168a06ebcd51543a5084" => :big_sur
    sha256 "a794407f3d43aab3b70acf278e6568a1fe610f9ce4d63f8bb8bc29afed5d1fd6" => :arm64_big_sur
    sha256 "ca966a273829f5903ba839d9a78b669bb7fa696b96df86b9296b615da76b97c6" => :catalina
    sha256 "b5d72dfd0e880c6c695869e11af4cdb67a1b9015fb3dfc2f26458021a6fc3795" => :mojave
  end

  depends_on "python@3.9"

  resource "colorama" do
    url "https://files.pythonhosted.org/packages/1f/bb/5d3246097ab77fa083a61bd8d3d527b7ae063c7d8e8671b1cf8c4ec10cbe/colorama-0.4.4.tar.gz"
    sha256 "5941b2b48a20143d2267e95b1c2a7603ce057ee39fd88e7329b0c292aa16869b"
  end

  resource "commonmark" do
    url "https://files.pythonhosted.org/packages/60/48/a60f593447e8f0894ebb7f6e6c1f25dafc5e89c5879fdc9360ae93ff83f0/commonmark-0.9.1.tar.gz"
    sha256 "452f9dc859be7f06631ddcb328b6919c67984aca654e5fefb3914d54691aed60"
  end

  resource "Pygments" do
    url "https://files.pythonhosted.org/packages/e1/86/8059180e8217299079d8719c6e23d674aadaba0b1939e25e0cc15dcf075b/Pygments-2.7.4.tar.gz"
    sha256 "df49d09b498e83c1a73128295860250b0b7edd4c723a32e9bc0d295c7c2ec337"
  end

  resource "typing-extensions" do
    url "https://files.pythonhosted.org/packages/16/06/0f7367eafb692f73158e5c5cbca1aec798cdf78be5167f6415dd4205fa32/typing_extensions-3.7.4.3.tar.gz"
    sha256 "99d4073b617d30288f569d3f13d2bd7548c3a7e4c8de87db09a9d29bb3a4a60c"
  end

  def install
    virtualenv_install_with_resources

    (bin/"richmd").write <<~SH
      #!/bin/bash
      #{libexec/"bin/python"} -m rich.markdown $@
    SH
  end

  test do
    assert_equal "• Hello, World", shell_output("echo '- Hello, World' | #{bin}/richmd").strip
  end
end
