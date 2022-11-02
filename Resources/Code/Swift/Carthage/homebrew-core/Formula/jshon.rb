class Jshon < Formula
  desc "Parse, read, and create JSON from the shell"
  homepage "http://kmkeen.com/jshon/"
  url "https://github.com/keenerd/jshon/archive/20131105.tar.gz"
  sha256 "28420f6f02c6b762732898692cc0b0795cfe1a59fbfb24e67b80f332cf6d4fa2"
  license "MIT"

  bottle do
    cellar :any
    sha256 "301ae5b178d603c79eb0ae2316647ea558c0eaea4331a525a9bc52f6f6387203" => :big_sur
    sha256 "cda0d78d58a0f23419cef2718919688bef98ec7461e750e60f9a10dd528c02ff" => :arm64_big_sur
    sha256 "ec755371878b6471b6d5fb0dac981f7969dad3a42b9aa2e9be60f7a303b3fffe" => :catalina
    sha256 "5aa1d640857651175a6fe692f31c0a28b58c7d5b02aac1dc6945c2fce7b76a81" => :mojave
    sha256 "c32084666ea13118a66803175575de9b74ca5a04d5a32bdd91883007ef6822b8" => :high_sierra
    sha256 "3215b76a79af85c6ae21b7de4e2eff0eb83098c0c5e1ae5b8c870d912498ed13" => :sierra
    sha256 "bab45017500667c7f8cf3b73c513f043cd04da04610cb2dc8a117ad5c9a5b99a" => :el_capitan
    sha256 "a97e9310af44fae5cdd60a0bcbae2bc0190c4d773d6290db3b5e970cd9999395" => :yosemite
  end

  depends_on "jansson"

  def install
    system "make"
    bin.install "jshon"
    man1.install "jshon.1"
  end

  test do
    (testpath/"test.json").write <<~EOS
      {"a":1,"b":2}
    EOS

    assert_equal "2", pipe_output("#{bin}/jshon -l < test.json").strip
  end
end
