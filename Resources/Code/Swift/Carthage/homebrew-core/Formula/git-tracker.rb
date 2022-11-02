class GitTracker < Formula
  desc "Integrate Pivotal Tracker into your Git workflow"
  homepage "https://github.com/stevenharman/git_tracker"
  url "https://github.com/stevenharman/git_tracker/archive/v2.0.0.tar.gz"
  sha256 "ec0a8d6dd056b8ae061d9ada08f1cc2db087e13aaecf4e0d150c1808e0250504"
  license "MIT"
  head "https://github.com/stevenharman/git_tracker.git"

  bottle do
    cellar :any_skip_relocation
    sha256 "cfb5a8706d8c08df443ae14911f6fded730dd1e5c3df7598e56ce110a74eea04" => :big_sur
    sha256 "f74c420720d35b85637c46102e5c897f099b7b0e049fb9114e757604c54d2da1" => :arm64_big_sur
    sha256 "4bfd3a9d45ba7fa4eb83c27efc7d56d4ceae11c2fcd51817d32024b05633a8e8" => :catalina
    sha256 "1b4a0022514c92d8aa724987024f38932e2dd897ffb85238e057977929d729c3" => :mojave
    sha256 "b99c9fa3e5e35a59659091162620299a1f39f6e36b25e05e71cdc2989157d66d" => :high_sierra
    sha256 "38f5240121fa53c038034ea9d0ce639f49c6df241727cbc5fbbd6acb48c60ea7" => :sierra
    sha256 "beba888de6c1dad6b4069be93ad1029ce70c24cc241c5b930c6aaf541fd11c4d" => :el_capitan
    sha256 "beba888de6c1dad6b4069be93ad1029ce70c24cc241c5b930c6aaf541fd11c4d" => :yosemite
  end

  def install
    system "rake", "standalone:install", "prefix=#{prefix}"
  end

  test do
    output = shell_output("#{bin}/git-tracker help")
    assert_match /git-tracker \d+(\.\d+)* is installed\./, output
  end
end
