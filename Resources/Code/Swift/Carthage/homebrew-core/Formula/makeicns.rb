class Makeicns < Formula
  desc "Create icns files from the command-line"
  homepage "https://bitbucket.org/mkae/makeicns"
  url "https://bitbucket.org/mkae/makeicns/downloads/makeicns-1.4.10a.tar.bz2"
  sha256 "10e44b8d84cb33ed8d92b9c2cfa42f46514586d2ec11ae9832683b69996ddeb8"
  head "https://bitbucket.org/mkae/makeicns", using: :hg

  bottle do
    cellar :any_skip_relocation
    sha256 "c2a5afff3eee709316951ad70c8244fe5c628ae98fdb2e15ea607c7638733d63" => :catalina
    sha256 "16d2135a49e22ffe920567c7ac382d5f706ef7ce5de377750553a0e59414819a" => :mojave
    sha256 "c40907f2d30603bdfe8402e90cbb35209b46cee1e7967d0ab06c21d5d7935eb8" => :high_sierra
    sha256 "3a673790c42724f75b905713e269f8bfa3e54bb64fde48130164c68b5656c871" => :sierra
    sha256 "96f91bccf728f040931c2816156a7c5de739ae91e63191795cd108d0a46370ac" => :el_capitan
    sha256 "40c3d4befe2d4625d7013ea40f307b4f5b26e122a6dad51706a25bb22734f075" => :yosemite
    sha256 "8c54ce9e5f819dda4eb274f8bf8a22d49e1d0086e33300f236840acf1a46837f" => :mavericks
  end

  disable! date: "2020-12-08", because: :repo_removed

  patch :p0 do
    url "https://raw.githubusercontent.com/Homebrew/formula-patches/e59da9d/makeicns/patch-IconFamily.m.diff"
    sha256 "f5ddbf6a688d6f153cf6fc2e15e75309adaf61677ab423cb67351e4fbb26066e"
  end

  def install
    system "make"
    bin.install "makeicns"
  end

  test do
    system bin/"makeicns", "-in", test_fixtures("test.png"),
           "-out", testpath/"test.icns"
    assert_predicate testpath/"test.icns", :exist?
  end
end
