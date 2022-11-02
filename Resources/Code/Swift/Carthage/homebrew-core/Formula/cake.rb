class Cake < Formula
  # As discussed with chenrui333 in this PR: https://github.com/Homebrew/homebrew-core/pull/55500#issuecomment-636390974
  # Cake uses the pre-release setting on all releases.  This will change
  # once we ship version 1.0.0, which is likely going to be our next release.
  desc "Cross platform build automation system with a C# DSL"
  homepage "https://cakebuild.net/"
  url "https://github.com/cake-build/cake/releases/download/v0.38.5/Cake-bin-net461-v0.38.5.zip"
  sha256 "7bda8369d90477288bda335484ab0905f1af8e780cd4b643f65e624491e2b456"
  license "MIT"

  bottle :unneeded

  depends_on "mono"

  conflicts_with "coffeescript", because: "both install `cake` binaries"

  def install
    libexec.install Dir["*.dll"]
    libexec.install Dir["*.exe"]
    libexec.install Dir["*.xml"]

    bin.mkpath
    (bin/"cake").write <<~EOS
      #!/bin/bash
      mono #{libexec}/Cake.exe "$@"
    EOS
  end

  test do
    (testpath/"build.cake").write <<~EOS
      var target = Argument ("target", "info");

      Task("info").Does(() =>
      {
        Information ("Hello Homebrew");
      });

      RunTarget ("info");
    EOS
    assert_match "Hello Homebrew\n", shell_output("#{bin}/cake build.cake")
  end
end
