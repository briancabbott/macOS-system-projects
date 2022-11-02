class Pipx < Formula
  desc "Execute binaries from Python packages in isolated environments"
  homepage "https://github.com/pipxproject/pipx"
  url "https://files.pythonhosted.org/packages/05/f6/6fb11e24d53686711bae5fed55ad2236b93386747676242e9614c8d00af2/pipx-0.15.6.0.tar.gz"
  sha256 "0d20e295a236b60e5601cb4e3d0c4fad202b9027ca1b2b8c88b322e66bf42b1f"
  license "MIT"
  head "https://github.com/pipxproject/pipx.git"

  livecheck do
    url :stable
  end

  bottle do
    cellar :any_skip_relocation
    sha256 "ba1011dbd0d219e53098dca70b3f6405e401166456fa5695a87623cc4544c85f" => :big_sur
    sha256 "6c0186ec981bd318f137c615c1b79ce0477f7f6410dce77dfff1e66dc3db3890" => :arm64_big_sur
    sha256 "88a34438842b955e46cc1c14430a0aba203b7bfdb5ccdceb3f796756c446c7bb" => :catalina
    sha256 "009a99c7c5338806125cf51cf4a2e2ad444c9302a64a9f09c7b9faf2a8229af5" => :mojave
    sha256 "206b537482465e4281356ccfbce84bfe01bed66530441017cc3cd2c8fa751a4d" => :high_sierra
  end

  depends_on "python@3.9"

  resource "argcomplete" do
    url "https://files.pythonhosted.org/packages/45/bd/98dfd56ea8f6b2b7dd89bea8b067a55a6dbaec7b4cc28186cbafe2e1d24e/argcomplete-1.12.1.tar.gz"
    sha256 "849c2444c35bb2175aea74100ca5f644c29bf716429399c0f2203bb5d9a8e4e6"
  end

  resource "click" do
    url "https://files.pythonhosted.org/packages/27/6f/be940c8b1f1d69daceeb0032fee6c34d7bd70e3e649ccac0951500b4720e/click-7.1.2.tar.gz"
    sha256 "d2b5255c7c6349bc1bd1e59e08cd12acbbd63ce649f2588755783aa94dfb6b1a"
  end

  resource "packaging" do
    url "https://files.pythonhosted.org/packages/55/fd/fc1aca9cf51ed2f2c11748fa797370027babd82f87829c7a8e6dbe720145/packaging-20.4.tar.gz"
    sha256 "4357f74f47b9c12db93624a82154e9b120fa8293699949152b22065d556079f8"
  end

  resource "pyparsing" do
    url "https://files.pythonhosted.org/packages/c1/47/dfc9c342c9842bbe0036c7f763d2d6686bcf5eb1808ba3e170afdb282210/pyparsing-2.4.7.tar.gz"
    sha256 "c203ec8783bf771a155b207279b9bccb8dea02d8f0c9e5f8ead507bc3246ecc1"
  end

  resource "six" do
    url "https://files.pythonhosted.org/packages/6b/34/415834bfdafca3c5f451532e8a8d9ba89a21c9743a0c59fbd0205c7f9426/six-1.15.0.tar.gz"
    sha256 "30639c035cdb23534cd4aa2dd52c3bf48f06e5f4a941509c8bafd8ce11080259"
  end

  resource "userpath" do
    url "https://files.pythonhosted.org/packages/86/2b/0a443e7978ea0f6bc1baece1de35545fa12f6d9fc5451aa90529db41db70/userpath-1.4.1.tar.gz"
    sha256 "211544ea02d8715fdc06f429cf66cd18c9877a31751d966d6de11b24faaed255"
  end

  def install
    xy = Language::Python.major_minor_version "python3"
    ENV.prepend_create_path "PYTHONPATH", libexec/"lib/python#{xy}/site-packages"

    resources.each do |r|
      r.stage do
        system "python3", *Language::Python.setup_install_args(libexec)
      end
    end

    system "python3", *Language::Python.setup_install_args(libexec)
    (bin/"pipx").write_env_script(libexec/"bin/pipx", PYTHONPATH: ENV["PYTHONPATH"])
    (bin/"register-python-argcomplete").write_env_script(libexec/"bin/register-python-argcomplete",
      PYTHONPATH: ENV["PYTHONPATH"])
  end

  test do
    assert_match "PIPX_HOME", shell_output("#{bin}/pipx --help")
    system "#{bin}/pipx", "install", "csvkit"
    assert_true FileTest.exist?("#{testpath}/.local/bin/csvjoin")
    system "#{bin}/pipx", "uninstall", "csvkit"
    assert_no_match Regexp.new("csvjoin"), shell_output("#{bin}/pipx list")
  end
end
