class Doitlive < Formula
  include Language::Python::Virtualenv

  desc "Replay stored shell commands for live presentations"
  homepage "https://doitlive.readthedocs.io/en/latest/"
  url "https://files.pythonhosted.org/packages/e5/d9/4ce969d98f521c253ec3b15a0c759104a01061ac90fb9d8636b015bcb4ea/doitlive-4.3.0.tar.gz"
  sha256 "4cb1030e082d8649f10a61d599d3ff3bcad7f775e08f0e68ee06882e06d0190f"
  revision 9

  livecheck do
    url :stable
  end

  bottle do
    cellar :any_skip_relocation
    sha256 "a5f6cb32662a07f6610df737c92935c8f94547edf61550ebc653046cd217c65b" => :big_sur
    sha256 "9f345a5cb4cbdc15aead62eb339617c77cfe572f0b49de0fcc8602fa96f7c321" => :arm64_big_sur
    sha256 "645c4c69758416768b1f853fba5d9e742c28985d8c1297aff866dcd05a84cce6" => :catalina
    sha256 "114d1d58d047ae404eb0a9280e11598359117b7cc832a993e2b4fd2a024a55e1" => :mojave
    sha256 "d17d744e85a69fbe1dccd0433b208aa554e9791866969a1fff484780150e24c6" => :high_sierra
  end

  depends_on "python@3.9"

  resource "click" do
    url "https://files.pythonhosted.org/packages/4e/ab/5d6bc3b697154018ef196f5b17d958fac3854e2efbc39ea07a284d4a6a9b/click-7.1.1.tar.gz"
    sha256 "8a18b4ea89d8820c5d0c7da8a64b2c324b4dabb695804dbfea19b9be9d88c0cc"
  end

  resource "click-completion" do
    url "https://files.pythonhosted.org/packages/93/18/74e2542defdda23b021b12b835b7abbd0fc55896aa8d77af280ad65aa406/click-completion-0.5.2.tar.gz"
    sha256 "5bf816b81367e638a190b6e91b50779007d14301b3f9f3145d68e3cade7bce86"
  end

  resource "click-didyoumean" do
    url "https://files.pythonhosted.org/packages/9f/79/d265d783dd022541b744d002745d9e55d84c04a41930e35d8795934f6526/click-didyoumean-0.0.3.tar.gz"
    sha256 "112229485c9704ff51362fe34b2d4f0b12fc71cc20f6d2b3afabed4b8bfa6aeb"
  end

  resource "Jinja2" do
    url "https://files.pythonhosted.org/packages/d8/03/e491f423379ea14bb3a02a5238507f7d446de639b623187bccc111fbecdf/Jinja2-2.11.1.tar.gz"
    sha256 "93187ffbc7808079673ef52771baa950426fd664d3aad1d0fa3e95644360e250"
  end

  resource "MarkupSafe" do
    url "https://files.pythonhosted.org/packages/b9/2e/64db92e53b86efccfaea71321f597fa2e1b2bd3853d8ce658568f7a13094/MarkupSafe-1.1.1.tar.gz"
    sha256 "29872e92839765e546828bb7754a68c418d927cd064fd4708fab9fe9c8bb116b"
  end

  resource "shellingham" do
    url "https://files.pythonhosted.org/packages/4b/f0/39516ebeaca978d6607609a283b15e7637622faffc5f01ecf78a49b24cd5/shellingham-1.3.2.tar.gz"
    sha256 "576c1982bea0ba82fb46c36feb951319d7f42214a82634233f58b40d858a751e"
  end

  resource "six" do
    url "https://files.pythonhosted.org/packages/21/9f/b251f7f8a76dec1d6651be194dfba8fb8d7781d10ab3987190de8391d08e/six-1.14.0.tar.gz"
    sha256 "236bdbdce46e6e6a3d61a337c0f8b763ca1e8717c03b369e87a7ec7ce1319c0a"
  end

  def install
    virtualenv_install_with_resources

    output = Utils.safe_popen_read({ "SHELL" => "bash" }, libexec/"bin/doitlive", "completion")
    (bash_completion/"doitlive").write output

    output = Utils.safe_popen_read({ "SHELL" => "zsh" }, libexec/"bin/doitlive", "completion")
    (zsh_completion/"_doitlive").write output
  end

  test do
    system "#{bin}/doitlive", "themes", "--preview"
  end
end
