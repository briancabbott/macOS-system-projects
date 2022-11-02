class Unnethack < Formula
  desc "Fork of Nethack"
  homepage "https://unnethack.wordpress.com/"
  url "https://github.com/UnNetHack/UnNetHack/archive/5.3.2.tar.gz"
  sha256 "a32a2c0e758eb91842033d53d43f718f3bc719a346e993d9b23bac06f0ac9004"
  head "https://github.com/UnNetHack/UnNetHack.git"

  bottle do
    sha256 "45d58053580ccdf9b65510768136206b71453b3457f23240a6dc592f817a6145" => :big_sur
    sha256 "5b4386eee78f20075e693b6ad437df496c8c914518161d8901991c1c4a6ee1f9" => :arm64_big_sur
    sha256 "5a1aea5f715d4c8892be4a5e76d60157da6637559a0055c41ea8024284807e91" => :catalina
    sha256 "84267cd44f073a41058516e7a8937da6b8b0f16e3500b0fd10ab0fedad77a5ce" => :mojave
    sha256 "47228cb416afe4d7e9ab31a2b85914e6b27f77e88340f7ef174bb2d9dd3ea2bb" => :high_sierra
  end

  # directory for temporary level data of running games
  skip_clean "var/unnethack/level"

  def install
    # directory for version specific files that shouldn't be deleted when
    # upgrading/uninstalling
    version_specific_directory = "#{var}/unnethack/#{version}"

    args = [
      "--prefix=#{prefix}",
      "--with-owner=#{`id -un`}",
      "--with-group=admin",
      # common xlogfile for all versions
      "--enable-xlogfile=#{var}/unnethack/xlogfile",
      "--with-bonesdir=#{version_specific_directory}/bones",
      "--with-savesdir=#{version_specific_directory}/saves",
      "--enable-wizmode=#{`id -un`}",
    ]

    system "./configure", *args
    ENV.deparallelize # Race condition in make

    # disable the `chgrp` calls
    system "make", "install", "CHGRP=#"
  end
end
