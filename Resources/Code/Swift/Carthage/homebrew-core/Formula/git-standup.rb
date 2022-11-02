class GitStandup < Formula
  desc "Git extension to generate reports for standup meetings"
  homepage "https://github.com/kamranahmedse/git-standup"
  url "https://github.com/kamranahmedse/git-standup/archive/2.3.2.tar.gz"
  sha256 "48d5aaa3c585037c950fa99dd5be8a7e9af959aacacde9fe94143e4e0bfcd6ba"
  license "MIT"
  head "https://github.com/kamranahmedse/git-standup.git"

  bottle do
    cellar :any_skip_relocation
    sha256 "39e65939c0bfd35248200c980400b99e72d4cf2054487d63c72c7e6b7268d3b0" => :big_sur
    sha256 "70ed7f5656e81453300e666c3db4c883ee9ef1f88206833b8eeb6b578fb56966" => :arm64_big_sur
    sha256 "0a75c65615d92237a59492ac00867d12ab4a23865d85d5cb464d9deb1f6d8ee8" => :catalina
    sha256 "0a75c65615d92237a59492ac00867d12ab4a23865d85d5cb464d9deb1f6d8ee8" => :mojave
    sha256 "0a75c65615d92237a59492ac00867d12ab4a23865d85d5cb464d9deb1f6d8ee8" => :high_sierra
  end

  def install
    system "make", "install", "PREFIX=#{prefix}"
  end

  test do
    system "git", "init"
    (testpath/"test").write "test"
    system "git", "add", "#{testpath}/test"
    system "git", "commit", "--message", "test"
    system "git", "standup"
  end
end
