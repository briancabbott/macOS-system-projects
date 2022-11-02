class Lab < Formula
  desc "Git wrapper for GitLab"
  homepage "https://zaquestion.github.io/lab"
  url "https://github.com/zaquestion/lab/archive/v0.18.0.tar.gz"
  sha256 "b34b08cb20d16f541eb3bf428e0224b4905ee40bda9394e7da4df756ba1aa109"
  license "CC0-1.0"
  head "https://github.com/zaquestion/lab.git"

  bottle do
    cellar :any_skip_relocation
    sha256 "91fb46c4c3bae4f629e69ab3f94088abeea42696832c895957a5c8548a08417a" => :big_sur
    sha256 "18f1c894f7d69729a4b558e5291188ded17e4025687d6b76c89f9000cd5b5402" => :arm64_big_sur
    sha256 "411d03a214b91d9b6fa403b5a34921967bbb75bd7facf916b9d313e7845488e9" => :catalina
    sha256 "0097c4d91eb8098827d8ed4c3a9bed00f33f30b2319a4c2a7ed82be5eb7efa25" => :mojave
  end

  depends_on "go" => :build

  # fix the build and remove in next release
  patch do
    url "https://github.com/prarit/lab/commit/4f7ea880d638647ec907f7e5e6395498588b7bcb.patch?full_index=1"
    sha256 "49e571927e5b85c226eacf55ad0b3918932ee526703fafb01002b541b011e80a"
  end

  def install
    ldflags = "-X main.version=#{version} -s -w"
    system "go", "build", *std_go_args, "-ldflags=#{ldflags}"
    output = Utils.safe_popen_read("#{bin}/lab", "completion", "bash")
    (bash_completion/"lab").write output
    output = Utils.safe_popen_read("#{bin}/lab", "completion", "zsh")
    (zsh_completion/"_lab").write output
    output = Utils.safe_popen_read("#{bin}/lab", "completion", "fish")
    (fish_completion/"lab.fish").write output
  end

  test do
    ENV["LAB_CORE_USER"] = "test_user"
    ENV["LAB_CORE_HOST"] = "https://gitlab.com"
    ENV["LAB_CORE_TOKEN"] = "dummy"

    ENV["GIT_AUTHOR_NAME"] = "test user"
    ENV["GIT_AUTHOR_EMAIL"] = "test@example.com"
    ENV["GIT_COMMITTER_NAME"] = "test user"
    ENV["GIT_COMMITTER_EMAIL"] = "test@example.com"

    system "git", "init"
    %w[haunted house].each { |f| touch testpath/f }
    system "git", "add", "haunted", "house"
    system "git", "commit", "-a", "-m", "Initial Commit"
    assert_match "haunted\nhouse", shell_output("#{bin}/lab ls-files").strip
  end
end
