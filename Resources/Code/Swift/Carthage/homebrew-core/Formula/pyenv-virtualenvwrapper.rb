class PyenvVirtualenvwrapper < Formula
  desc "Alternative to pyenv for managing virtualenvs"
  homepage "https://github.com/pyenv/pyenv-virtualenvwrapper"
  url "https://github.com/pyenv/pyenv-virtualenvwrapper/archive/v20140609.tar.gz"
  sha256 "c1c812c4954394c58628952654ba745c4fb814d045adc076f7fb9e310bed03bf"
  license "MIT"
  head "https://github.com/pyenv/pyenv-virtualenvwrapper.git"

  bottle :unneeded

  depends_on "pyenv"

  def install
    ENV["PREFIX"] = prefix
    system "./install.sh"
  end

  test do
    shell_output("eval \"$(pyenv init -)\" && pyenv virtualenvwrapper")
  end
end
