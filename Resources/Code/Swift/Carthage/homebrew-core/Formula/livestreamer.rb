class Livestreamer < Formula
  include Language::Python::Virtualenv

  desc "Pipes video from streaming services into a player such as VLC"
  homepage "https://livestreamer.io/"
  url "https://files.pythonhosted.org/packages/ee/d6/efbe3456160a2c62e3dd841c5d9504d071c94449a819148bb038b50d862a/livestreamer-1.12.2.tar.gz"
  sha256 "ef3e743d0cabc27d8ad906c356e74370799e25ba46c94d3b8d585af77a258de0"
  revision 3

  livecheck do
    url :stable
  end

  bottle do
    cellar :any_skip_relocation
    sha256 "b980aa79f3deaf175618c8463eee5aaca734fbd3975dfb17f0f0dffe34ec7685" => :big_sur
    sha256 "737283439fdddac90ef1240a125883d41e14f39fdb32c03f561f8de5bfcac294" => :arm64_big_sur
    sha256 "fb66f50467554c547b0dc938621146994dce5069ac01f8b55badbb6363e8f11d" => :catalina
    sha256 "4e01e88af3e7134c460b64e0405093b345d3a60130a1792b7a3d371bac089370" => :mojave
    sha256 "7d52881a8c42d16e27bd03a5b678b4b6a3ad8a93179dd9cee9507c8a9676859b" => :high_sierra
  end

  depends_on "python@3.9"

  resource "certifi" do
    url "https://files.pythonhosted.org/packages/b8/e2/a3a86a67c3fc8249ed305fc7b7d290ebe5e4d46ad45573884761ef4dea7b/certifi-2020.4.5.1.tar.gz"
    sha256 "51fcb31174be6e6664c5f69e3e1691a2d72a1a12e90f872cbdb1567eb47b6519"
  end

  resource "chardet" do
    url "https://files.pythonhosted.org/packages/fc/bb/a5768c230f9ddb03acc9ef3f0d4a3cf93462473795d18e9535498c8f929d/chardet-3.0.4.tar.gz"
    sha256 "84ab92ed1c4d4f16916e05906b6b75a6c0fb5db821cc65e70cbd64a3e2a5eaae"
  end

  resource "idna" do
    url "https://files.pythonhosted.org/packages/cb/19/57503b5de719ee45e83472f339f617b0c01ad75cba44aba1e4c97c2b0abd/idna-2.9.tar.gz"
    sha256 "7588d1c14ae4c77d74036e8c22ff447b26d0fde8f007354fd48a7814db15b7cb"
  end

  resource "requests" do
    url "https://files.pythonhosted.org/packages/f5/4f/280162d4bd4d8aad241a21aecff7a6e46891b905a4341e7ab549ebaf7915/requests-2.23.0.tar.gz"
    sha256 "b3f43d496c6daba4493e7c431722aeb7dbc6288f52a6e04e7b6023b0247817e6"
  end

  resource "urllib3" do
    url "https://files.pythonhosted.org/packages/05/8c/40cd6949373e23081b3ea20d5594ae523e681b6f472e600fbc95ed046a36/urllib3-1.25.9.tar.gz"
    sha256 "3018294ebefce6572a474f0604c2021e33b3fd8006ecd11d62107a5d2a963527"
  end

  def install
    virtualenv_install_with_resources
  end

  test do
    assert_match version.to_s, shell_output("#{bin}/livestreamer --version 2>&1")
  end
end
