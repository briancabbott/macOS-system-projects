class SqliteUtils < Formula
  include Language::Python::Virtualenv
  desc "CLI utility for manipulating SQLite databases"
  homepage "https://sqlite-utils.datasette.io/"
  url "https://files.pythonhosted.org/packages/ad/5e/420def7ab0eb2b64d742384b4e0bbac83041617482ee875a1a49ae8d46ac/sqlite-utils-3.2.1.tar.gz"
  sha256 "34e3332ea84c801684bd8d1c54b5605a7cdd4bf3706d11b9ae1b7319e1fe4218"
  license "Apache-2.0"

  bottle do
    cellar :any_skip_relocation
    sha256 "f6b2df4fee426137127f4e954b320eac2abbd31ddffbf023cb914e77078618b8" => :big_sur
    sha256 "c1ae23316cf1e72d8dacc65351a677817a8a289df37273f237e1f3cf8657b31b" => :arm64_big_sur
    sha256 "a01514ef3a50450d8748f93a759a6de5bac9b24867e5ff3b8ceb42faf3c982b4" => :catalina
    sha256 "b0b6c0d8e9777964ccc631dea912e4f348717747abc671a19c4d1846766f31aa" => :mojave
  end

  depends_on "python@3.9"

  resource "click" do
    url "https://files.pythonhosted.org/packages/27/6f/be940c8b1f1d69daceeb0032fee6c34d7bd70e3e649ccac0951500b4720e/click-7.1.2.tar.gz"
    sha256 "d2b5255c7c6349bc1bd1e59e08cd12acbbd63ce649f2588755783aa94dfb6b1a"
  end

  resource "click-default-group" do
    url "https://files.pythonhosted.org/packages/22/3a/e9feb3435bd4b002d183fcb9ee08fb369a7e570831ab1407bc73f079948f/click-default-group-1.2.2.tar.gz"
    sha256 "d9560e8e8dfa44b3562fbc9425042a0fd6d21956fcc2db0077f63f34253ab904"
  end

  resource "sqlite-fts4" do
    url "https://files.pythonhosted.org/packages/62/30/63e64b7b8fa69aabf97b14cbc204cb9525eb2132545f82231c04a6d40d5c/sqlite-fts4-1.0.1.tar.gz"
    sha256 "b2d4f536a28181dc4ced293b602282dd982cc04f506cf3fc491d18b824c2f613"
  end

  resource "tabulate" do
    url "https://files.pythonhosted.org/packages/57/6f/213d075ad03c84991d44e63b6516dd7d185091df5e1d02a660874f8f7e1e/tabulate-0.8.7.tar.gz"
    sha256 "db2723a20d04bcda8522165c73eea7c300eda74e0ce852d9022e0159d7895007"
  end

  def install
    virtualenv_install_with_resources
  end

  test do
    assert_match "15", shell_output("#{bin}/sqlite-utils :memory: 'select 3 * 5'")
  end
end
