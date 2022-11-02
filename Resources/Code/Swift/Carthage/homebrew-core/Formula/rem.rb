class Rem < Formula
  desc "Command-line tool to access OSX Reminders.app database"
  homepage "https://github.com/kykim/rem"
  url "https://github.com/kykim/rem/archive/20150618.tar.gz"
  sha256 "e57173a26d2071692d72f3374e36444ad0b294c1284e3b28706ff3dbe38ce8af"
  license "Apache-2.0"

  bottle do
    cellar :any_skip_relocation
    sha256 "e0af2d48a7809890f04480b0b4d28f6354130754609627ed76ce6d76a5135898" => :big_sur
    sha256 "f0f72e190a73fd43f3528c08049aca2f52ce7a8cfa25b779383c3692ee7aab18" => :arm64_big_sur
    sha256 "bfab3fd2fd8da4e4620d80a632d774b4742c6c34c5b73d89fafd3d246369fce6" => :catalina
    sha256 "4226be6dc999a4467a061055cb36a68babe84a835f40f32a5a23f6137ddd59b4" => :mojave
    sha256 "0a3365c8653023f2b4de8c5b6243aec2de7c180d1be982adcdbe58afc159800e" => :high_sierra
    sha256 "326f7a21f696b7614a55a5edeb57e08482ff7b4c72506bcecff5deaa0552828e" => :sierra
    sha256 "c9892df4f6aa5d58097e4cc4d62388ccbb1e0c02604b1139cfe829d47d992442" => :el_capitan
    sha256 "d9a6303ff3935923ba53d093e95387caaf24460a4cd7fb7d330fa5c3988b551c" => :yosemite
    sha256 "bf65e89ec4ca486b95f04c1c737627b2e0091af8a5c137795e521b96664d75e2" => :mavericks
  end

  depends_on xcode: :build

  conflicts_with "remind", because: "both install `rem` binaries"

  def install
    xcodebuild "SYMROOT=build"
    bin.install "build/Release/rem"
  end

  test do
    system "#{bin}/rem", "version"
  end
end
