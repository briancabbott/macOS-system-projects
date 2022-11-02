class GitArchiveAll < Formula
  desc "Archive a project and its submodules"
  homepage "https://github.com/Kentzo/git-archive-all"
  url "https://github.com/Kentzo/git-archive-all/archive/1.22.0.tar.gz"
  sha256 "3eef66c5af010f75d4d270618ecbfdb670bde14e39bdfeed0bab3a5d12c7d6a2"
  license "MIT"
  head "https://github.com/Kentzo/git-archive-all.git"

  bottle do
    cellar :any_skip_relocation
    sha256 "13418fa4c3212278d566d1b4ee0a43f62727c71dfea8c268a8994e6aeb33d8f0" => :big_sur
    sha256 "90de61e5da3d36a5c97afbc002d0f65ad88092a5551ce36e10e7d2901045f16b" => :arm64_big_sur
    sha256 "d83a7c27f97788c8a76ba8fc708a30795e6bede82329dd32e39a8dee6b907a79" => :catalina
    sha256 "d83a7c27f97788c8a76ba8fc708a30795e6bede82329dd32e39a8dee6b907a79" => :mojave
    sha256 "d83a7c27f97788c8a76ba8fc708a30795e6bede82329dd32e39a8dee6b907a79" => :high_sierra
  end

  def install
    system "make", "prefix=#{prefix}", "install"
  end

  test do
    (testpath/".gitconfig").write <<~EOS
      [user]
        name = Real Person
        email = notacat@hotmail.cat
    EOS
    system "git", "init"
    touch "homebrew"
    system "git", "add", "homebrew"
    system "git", "commit", "--message", "brewing"

    assert_equal "#{testpath.realpath}/homebrew => archive/homebrew",
                 shell_output("#{bin}/git-archive-all --dry-run ./archive").chomp
  end
end
