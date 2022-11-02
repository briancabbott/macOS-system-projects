class Swimat < Formula
  desc "Command-line tool to help format Swift code"
  homepage "https://github.com/Jintin/Swimat"
  url "https://github.com/Jintin/Swimat/archive/1.7.0.tar.gz"
  sha256 "ba18b628de8b0a679b9215fb77e313155430fbecd21b15ed5963434223b10046"
  license "MIT"
  head "https://github.com/Jintin/Swimat.git"

  livecheck do
    url :stable
    strategy :github_latest
  end

  bottle do
    cellar :any_skip_relocation
    sha256 "f4099d895297155fe34b95ff66e214c31fcf2990e03aeaad8e1680061fb580a9" => :big_sur
    sha256 "5474723b7d13050f04c03a9ba5dc7bf0d962b13ff384f82fd9cf5f47754fdb7a" => :arm64_big_sur
    sha256 "6ee6f59882dcec7188ef4684fcada0d22edf68470023fffb73b610f2dbe44112" => :catalina
    sha256 "6b9a5174b6050250d0dfe5721102c5455997f2abcef1f2dc6a82686af11117fd" => :mojave
  end

  depends_on xcode: ["10.2", :build]

  def install
    xcodebuild "-target", "CLI",
               "-configuration", "Release",
               "CODE_SIGN_IDENTITY=",
               "SYMROOT=build"
    bin.install "build/Release/swimat"
  end

  test do
    system "#{bin}/swimat", "-h"
    (testpath/"SwimatTest.swift").write("struct SwimatTest {}")
    system "#{bin}/swimat", "#{testpath}/SwimatTest.swift"
  end
end
