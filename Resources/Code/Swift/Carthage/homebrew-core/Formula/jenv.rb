class Jenv < Formula
  desc "Manage your Java environment"
  homepage "https://www.jenv.be/"
  url "https://github.com/jenv/jenv/archive/0.5.4.tar.gz"
  sha256 "15a78dab7310fb487d2c2cad7f69e05d5d797dc13f2d5c9e7d0bbec4ea3f2980"
  license "MIT"

  head "https://github.com/jenv/jenv.git"

  bottle :unneeded

  def install
    libexec.install Dir["*"]
    bin.write_exec_script libexec/"bin/jenv"
  end

  def caveats
    <<~EOS
      To activate jenv, add the following to your #{shell_profile}:

        export PATH="$HOME/.jenv/bin:$PATH"
        eval "$(jenv init -)"
    EOS
  end

  test do
    shell_output("eval \"$(#{bin}/jenv init -)\" && jenv versions")
  end
end
