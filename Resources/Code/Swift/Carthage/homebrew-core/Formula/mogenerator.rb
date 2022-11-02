class Mogenerator < Formula
  desc "Generate Objective-C & Swift classes from your Core Data model"
  homepage "https://rentzsch.github.io/mogenerator/"
  url "https://github.com/rentzsch/mogenerator/archive/1.32.tar.gz"
  sha256 "4fa660a19934d94d7ef35626d68ada9912d925416395a6bf4497bd7df35d7a8b"
  license "MIT"
  head "https://github.com/rentzsch/mogenerator.git"

  bottle do
    cellar :any_skip_relocation
    rebuild 1
    sha256 "415e0e160574b7b16dff3d0395a7e156894675191c911d09cddf59e1d916571b" => :big_sur
    sha256 "51aec3a49207ae357af26a5407494bc88d98027ba06293736b2888ece7b1d71c" => :arm64_big_sur
    sha256 "d62cad0cc94a7b05286fb2a8a2f8e4a4cc3a9b46efa9a391aa9fcb00c381e85e" => :catalina
    sha256 "dcb658659b696e44f13e382f553c92199a7ab0be48ff69f33a35ef98ee8a09ac" => :mojave
  end

  depends_on xcode: :build

  # https://github.com/rentzsch/mogenerator/pull/390
  patch do
    url "https://github.com/rentzsch/mogenerator/commit/20d9cce6df8380160cac0ce07687688076fddf3d.patch?full_index=1"
    sha256 "de700f06c32cc0d4fbcb1cdd91e9e97a55931bc047841985d5c0905e65b5e5b0"
  end

  def install
    xcodebuild "-target", "mogenerator", "-configuration", "Release", "SYMROOT=symroot", "OBJROOT=objroot"
    bin.install "symroot/Release/mogenerator"
  end

  test do
    system "#{bin}/mogenerator", "--version"
  end
end
