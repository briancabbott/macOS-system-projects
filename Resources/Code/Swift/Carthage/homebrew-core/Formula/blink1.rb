class Blink1 < Formula
  desc "Control blink(1) indicator light"
  homepage "https://blink1.thingm.com/"
  url "https://github.com/todbot/blink1-tool.git",
      tag:      "v2.1.0",
      revision: "6aed8b1c334edcbcff04951db4bba7816ef336f7"
  license "CC-BY-SA-3.0"
  head "https://github.com/todbot/blink1-tool.git"

  bottle do
    cellar :any
    sha256 "669d71aaa543c504392611cbaeecd2b2a0f1d10dad18f297a85152ea544bbb26" => :big_sur
    sha256 "abc11edfdb32a4968678c20a5ccf85d8180966d7aed0b30a0fbb14cee055477f" => :arm64_big_sur
    sha256 "fe997b4328928968cb091e9d748213160a1a0f7f4644621b14cc1a4bf7e06680" => :catalina
    sha256 "4168f9d980daf9f4864426a4447ba004efb492289d9fc8893b49f99d581a8e00" => :mojave
    sha256 "646191cfd60d1a6d332c536bf3e50914a2434d72afee888c40568eebd7617d35" => :high_sierra
  end

  def install
    system "make"
    bin.install "blink1-tool"
    lib.install "libBlink1.dylib"
    include.install "blink1-lib.h"
  end

  test do
    system bin/"blink1-tool", "--version"
  end
end
