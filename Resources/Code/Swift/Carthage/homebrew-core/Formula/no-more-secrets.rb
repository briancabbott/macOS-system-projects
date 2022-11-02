class NoMoreSecrets < Formula
  desc "Recreates the SETEC ASTRONOMY effect from 'Sneakers'"
  homepage "https://github.com/bartobri/no-more-secrets"
  url "https://github.com/bartobri/no-more-secrets/archive/v0.3.3.tar.gz"
  sha256 "cfcf408768c6b335780e46a84fbc121a649c4b87e0564fc972270e96630efdce"
  license "GPL-3.0"

  bottle do
    cellar :any_skip_relocation
    sha256 "884cb0503a1014e64fe9d310015c8eafc83f0980fb395da51cf895dd8e40faac" => :big_sur
    sha256 "f05fcfd3fc3c3cac082c2d0fabb206024fa83e2e834af3102ac6cca44563c612" => :arm64_big_sur
    sha256 "0a47f3f151de373eeb54010f4f5fa3db680866f740a25231452852a22fe3477c" => :catalina
    sha256 "bf89c9bc341d6dc82bfbb242b6414a2f778b0bc1c26e5f4ced239c649902aad6" => :mojave
    sha256 "ad2927337af4e85d6bff3fbdcfeb2e435c85de8d527d23a3644c7add3c7acab0" => :high_sierra
    sha256 "97ff320dd7639a7a71fbfa4f7e72fb7c66e4b60ea0f6a6adc4583c63cbda05ac" => :sierra
    sha256 "78c52bd9f179967cb240c8f49763e03e512092ee476b73e38166bfa79757664f" => :el_capitan
  end

  def install
    system "make", "all"
    system "make", "prefix=#{prefix}", "install"
  end

  test do
    assert_equal "nms version #{version}", shell_output("#{bin}/nms -v").chomp
  end
end
