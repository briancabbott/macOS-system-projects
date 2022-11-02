class Couchpotatoserver < Formula
  desc "Download movies automatically"
  homepage "https://couchpota.to"
  url "https://github.com/CouchPotato/CouchPotatoServer/archive/build/3.0.1.tar.gz"
  sha256 "f08f9c6ac02f66c6667f17ded1eea4c051a62bbcbadd2a8673394019878e92f7"
  license "GPL-3.0"
  head "https://github.com/CouchPotato/CouchPotatoServer.git"

  bottle :unneeded

  def install
    prefix.install_metafiles
    libexec.install Dir["*"]
    (bin+"couchpotatoserver").write(startup_script)
  end

  def caveats
    "CouchPotatoServer defaults to port 5050."
  end

  plist_options manual: "couchpotatoserver"

  def plist
    <<~EOS
      <!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
      <plist version="1.0">
        <dict>
          <key>Label</key>
          <string>#{plist_name}</string>
          <key>Program</key>
          <string>#{opt_bin}/couchpotatoserver</string>
          <key>ProgramArguments</key>
          <array>
            <string>--quiet</string>
            <string>--daemon</string>
          </array>
          <key>RunAtLoad</key>
          <true/>
          <key>UserName</key>
          <string>#{`whoami`.chomp}</string>
        </dict>
      </plist>
    EOS
  end

  def startup_script
    <<~EOS
      #!/bin/bash
      python "#{libexec}/CouchPotato.py"\
             "--pid_file=#{var}/run/couchpotatoserver.pid"\
             "--data_dir=#{etc}/couchpotatoserver"\
             "$@"
    EOS
  end

  test do
    system "#{bin}/couchpotatoserver", "--help"
  end
end
