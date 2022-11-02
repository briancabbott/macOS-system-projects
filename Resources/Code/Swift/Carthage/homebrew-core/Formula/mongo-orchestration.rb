class MongoOrchestration < Formula
  include Language::Python::Virtualenv

  desc "REST API to manage MongoDB configurations on a single host"
  homepage "https://github.com/10gen/mongo-orchestration"
  url "https://files.pythonhosted.org/packages/72/34/9f010c4ac8569314569ea69a93a234d1dedf211666ab0b01b919d7843dba/mongo-orchestration-0.6.12.tar.gz"
  sha256 "d73f7666424ee6e4b2143c1e2f72025b15236dacd07c80a374a44bb056d53a6b"
  license "Apache-2.0"
  revision 1
  head "https://github.com/10gen/mongo-orchestration.git"

  livecheck do
    url :stable
  end

  bottle do
    cellar :any_skip_relocation
    sha256 "33b8aa1ce79e8b2255b39c2d39991c4289463c6c98bb7a924ab5c921253d4209" => :big_sur
    sha256 "caea2e09658e9e24c50411755c5f24763f1f445e0e7afad9d99dc14b324df2c1" => :arm64_big_sur
    sha256 "53243e66a4c636eb3add6e08caa9778097f0a77decfbc3d1c30e24530b12f32d" => :catalina
    sha256 "b780ed0619d226f5c212347fb5825d216605418fd081b4faebe74246fc5201a6" => :mojave
    sha256 "333144d847b2581b86d1d2a607786e428a1d2a7b55c45f776d405b76bd13ae77" => :high_sierra
  end

  depends_on "python@3.9"

  resource "bottle" do
    url "https://files.pythonhosted.org/packages/d9/4f/57887a07944140dae0d039d8bc270c249fc7fc4a00744effd73ae2cde0a9/bottle-0.12.18.tar.gz"
    sha256 "0819b74b145a7def225c0e83b16a4d5711fde751cd92bae467a69efce720f69e"
  end

  resource "CherryPy" do
    url "https://files.pythonhosted.org/packages/50/c6/6c3d7a3221b0f098f8684037736e5604ea1586a3ba450c4a52b48f5fc2b4/CherryPy-7.0.0.tar.gz"
    sha256 "faead7c5c7ca2526aff8f179a24d699127ed307c3393eeef9610a33b93650bef"
  end

  resource "pymongo" do
    url "https://files.pythonhosted.org/packages/a8/f6/f324f5c669478644ac64594b9d746a34e185d9c34d3f05a4a6a6dab5467b/pymongo-3.5.1.tar.gz"
    sha256 "e820d93414f3bec1fa456c84afbd4af1b43ff41366321619db74e6bc065d6924"
  end

  resource "six" do
    url "https://files.pythonhosted.org/packages/16/d8/bc6316cf98419719bd59c91742194c111b6f2e85abac88e496adefaf7afe/six-1.11.0.tar.gz"
    sha256 "70e8a77beed4562e7f14fe23a786b54f6296e34344c23bc42f07b15018ff98e9"
  end

  def install
    virtualenv_install_with_resources
  end

  plist_options startup: true, manual: "#{HOMEBREW_PREFIX}/opt/mongo-orchestration/bin/mongo-orchestration -b 127.0.0.1 -p 8889 --no-fork start"

  def plist
    <<~EOS
      <?xml version="1.0" encoding="UTF-8"?>
      <!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
      <plist version="1.0">
        <dict>
          <key>Label</key>
          <string>mongo-orchestration</string>
          <key>ProgramArguments</key>
          <array>
            <string>#{opt_bin}/mongo-orchestration</string>
            <string>-b</string>
            <string>127.0.0.1</string>
            <string>-p</string>
            <string>8889</string>
            <string>--no-fork</string>
            <string>start</string>
          </array>
          <key>RunAtLoad</key>
          <true/>
        </dict>
      </plist>
    EOS
  end

  test do
    system "#{bin}/mongo-orchestration", "-h"
  end
end
