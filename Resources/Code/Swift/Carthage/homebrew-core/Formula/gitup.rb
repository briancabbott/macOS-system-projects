class Gitup < Formula
  include Language::Python::Virtualenv

  desc "Update multiple git repositories at once"
  homepage "https://github.com/earwig/git-repo-updater"
  url "https://files.pythonhosted.org/packages/7f/07/4835f8f4de5924b5f38b816c648bde284f0cec9a9ae65bd7e5b7f5867638/gitup-0.5.1.tar.gz"
  sha256 "4f787079cd65d8f60c5842181204635e1b72d3533ae91f0c619624c6b20846dd"
  license "MIT"
  revision 3
  head "https://github.com/earwig/git-repo-updater.git"

  livecheck do
    url :stable
  end

  bottle do
    cellar :any_skip_relocation
    rebuild 1
    sha256 "75267b1b78d901d02d25d3ffb0f867a5428319a5e7f71852549e610d92aef4e9" => :big_sur
    sha256 "cac513baa7ac0514017dbb53996278e12f42cb9f8995c5404e332d4e742260f0" => :arm64_big_sur
    sha256 "24a5b0141173ca61a85b8b7aeec68dce1d43d17d67e7edf92d4197241b977c91" => :catalina
    sha256 "2ef7af3d0f8928ef352902bb7d583684adda069580d13af4c3d26818e5808d3a" => :mojave
  end

  depends_on "python@3.9"

  resource "colorama" do
    url "https://files.pythonhosted.org/packages/1f/bb/5d3246097ab77fa083a61bd8d3d527b7ae063c7d8e8671b1cf8c4ec10cbe/colorama-0.4.4.tar.gz"
    sha256 "5941b2b48a20143d2267e95b1c2a7603ce057ee39fd88e7329b0c292aa16869b"
  end

  resource "gitdb" do
    url "https://files.pythonhosted.org/packages/d1/05/eaf2ac564344030d8b3ce870b116d7bb559020163e80d9aa4a3d75f3e820/gitdb-4.0.5.tar.gz"
    sha256 "c9e1f2d0db7ddb9a704c2a0217be31214e91a4fe1dea1efad19ae42ba0c285c9"
  end

  resource "GitPython" do
    url "https://files.pythonhosted.org/packages/85/3d/ee9aa9c77a3c0e9074461d2d8da86c3564ed96abd28fa099dc3e05338a72/GitPython-3.1.11.tar.gz"
    sha256 "befa4d101f91bad1b632df4308ec64555db684c360bd7d2130b4807d49ce86b8"
  end

  resource "smmap" do
    url "https://files.pythonhosted.org/packages/75/fb/2f594e5364f9c986b2c89eb662fc6067292cb3df2b88ae31c939b9138bb9/smmap-3.0.4.tar.gz"
    sha256 "9c98bbd1f9786d22f14b3d4126894d56befb835ec90cef151af566c7e19b5d24"
  end

  def install
    virtualenv_install_with_resources
  end

  test do
    def prepare_repo(uri, local_head)
      system "git", "init"
      system "git", "remote", "add", "origin", uri
      system "git", "fetch", "origin"
      system "git", "checkout", local_head
      system "git", "reset", "--hard"
      system "git", "checkout", "-b", "master"
      system "git", "branch", "--set-upstream-to=origin/master", "master"
    end

    first_head_start = "f47ab45abdbc77e518776e5dc44f515721c523ae"
    mkdir "first" do
      prepare_repo("https://github.com/pr0d1r2/homebrew-contrib.git", first_head_start)
    end

    second_head_start = "f863d5ca9e39e524e8c222428e14625a5053ed2b"
    mkdir "second" do
      prepare_repo("https://github.com/pr0d1r2/homebrew-cask-games.git", second_head_start)
    end

    system bin/"gitup", "first", "second"

    first_head = `cd first ; git rev-parse HEAD`.split.first
    assert_not_equal first_head, first_head_start

    second_head = `cd second ; git rev-parse HEAD`.split.first
    assert_not_equal second_head, second_head_start

    third_head_start = "f47ab45abdbc77e518776e5dc44f515721c523ae"
    mkdir "third" do
      prepare_repo("https://github.com/pr0d1r2/homebrew-contrib.git", third_head_start)
    end

    system bin/"gitup", "--add", "third"

    system bin/"gitup"
    third_head = `cd third ; git rev-parse HEAD`.split.first
    assert_not_equal third_head, third_head_start

    assert_match %r{#{Dir.pwd}/third}, `#{bin}/gitup --list`.strip

    system bin/"gitup", "--delete", "#{Dir.pwd}/third"
  end
end
