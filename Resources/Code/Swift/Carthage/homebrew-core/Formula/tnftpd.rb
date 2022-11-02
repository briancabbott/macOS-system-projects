class Tnftpd < Formula
  desc "NetBSD's FTP server"
  homepage "https://ftp.netbsd.org/pub/NetBSD/misc/tnftp/"
  url "https://ftp.netbsd.org/pub/NetBSD/misc/tnftp/tnftpd-20200704.tar.gz"
  sha256 "92de915e1b4b7e4bd403daac5d89ce67fa73e49e8dda18e230fa86ee98e26ab7"

  livecheck do
    url :homepage
    regex(/href=.*?tnftpd[._-]v?(\d+)\.t/i)
  end

  bottle do
    cellar :any_skip_relocation
    sha256 "05728c1edd46c07fe6e19d54094d53dc78614cae7b04320794a9e4ba43dad099" => :big_sur
    sha256 "cbc7f23e857584e25c7d2d043a3971841febe99f12830d82cf28fe47a2e9e254" => :catalina
    sha256 "3e8848729081c09a247e0326ede175db12111360905f69cc339dea3ba0213e62" => :mojave
    sha256 "18a15c1572f7f5b33b7678d9a322de20efcd0c1b1c5c98d8cb00e13a80bfa518" => :high_sierra
  end

  def install
    system "./configure", "--prefix=#{prefix}"
    system "make"

    sbin.install "src/tnftpd" => "ftpd"
    man8.install "src/tnftpd.man" => "ftpd.8"
    man5.install "src/ftpusers.man" => "ftpusers.5"
    man5.install "src/ftpd.conf.man" => "ftpd.conf.5"
    etc.install "examples/ftpd.conf"
    etc.install "examples/ftpusers"
    prefix.install_metafiles
  end

  def caveats
    <<~EOS
      You may need super-user privileges to run this program properly. See the man
      page for more details.
    EOS
  end

  test do
    # running a whole server, connecting, and so forth is a bit clunky and hard
    # to write properly so...
    require "pty"
    require "expect"

    PTY.spawn "#{sbin}/ftpd -x" do |input, _output, _pid|
      str = input.expect(/ftpd: illegal option -- x/)
      assert_match "ftpd: illegal option -- x", str[0]
    end
  end
end
