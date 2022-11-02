class WakatimeCli < Formula
  desc "Command-line interface to the WakaTime api"
  homepage "https://wakatime.com/"
  url "https://files.pythonhosted.org/packages/87/03/1e919ddff6488a9be5e609ee3de500bf17b0151734d57c8923c605246235/wakatime-13.1.0.tar.gz"
  sha256 "8da2e38335a60819b61adb60a72adc6588b3755eca376d05c9588cbf5fe64c57"
  license "BSD-3-Clause"

  livecheck do
    url :stable
  end

  bottle do
    cellar :any_skip_relocation
    sha256 "3435709410408bacf076594f87fffe654048e4e7aabe124f7f6bb37bbf3943e9" => :big_sur
    sha256 "062307ba64d4afeda4bdae8c912a9c69d5e28078f7e4169aafb0c14c355cd297" => :arm64_big_sur
    sha256 "1573e0dd92f96002d51d388bb75f4ea06946dacf8c2e46c2408513c0a13c9feb" => :catalina
    sha256 "84e365ad5241e4c17926bb32730cbf0d2d9de798551e137fe568a3934e7d733f" => :mojave
    sha256 "9c4ddbce30fc3b94deb970c30527a80534e4389810524cfc58b634fc0863fc0c" => :high_sierra
  end

  depends_on "python@3.9"

  def install
    xy = Language::Python.major_minor_version "python3"
    ENV["PYTHONPATH"] = libexec/"lib/python#{xy}/site-packages"

    system "python3", *Language::Python.setup_install_args(libexec)
    bin.install Dir[libexec/"bin/*"]
    bin.env_script_all_files(libexec/"bin", PYTHONPATH: ENV["PYTHONPATH"])
  end

  test do
    assert_match "Common interface for the WakaTime api.", shell_output("#{bin}/wakatime --help 2>&1")

    assert_match "error: Missing api key.", shell_output("#{bin}/wakatime --project test 2>&1", 104)
  end
end
