class Ircii < Formula
  desc "IRC and ICB client"
  homepage "http://www.eterna.com.au/ircii/"
  url "https://ircii.warped.com/ircii-20190117.tar.bz2"
  mirror "https://deb.debian.org/debian/pool/main/i/ircii/ircii_20190117.orig.tar.bz2"
  sha256 "10316f0a3723e4ce3d67fd5a7df10e6bcf30dd0750fb96d5437cacb16b0e9617"
  revision 1

  livecheck do
    url "https://ircii.warped.com/"
    regex(/href=.*?ircii[._-]v?(\d{6,8})\.t/i)
  end

  bottle do
    sha256 "5fa9235fe1b9f86202dabe7b7a62a0e5fc0007e7f211d50ce04c5574dba30af0" => :big_sur
    sha256 "a2cd8fdd4ad9a08296a510e61772422d95ef1e047405c79d613a4e46ce4e68e3" => :arm64_big_sur
    sha256 "fea5e21aa100bda1104f1d4947376e9af68be118caf89d8b9468b51cbd2059b4" => :catalina
    sha256 "95424df7e321088386df0d493fdc9a29c0f1955201ce0ce6ba9640a6c9678a9e" => :mojave
    sha256 "ff375e444386f89ee0ae5f3aa3b6f38bfaa8818c3cd2db8065669f8a0f7d0165" => :high_sierra
    sha256 "d0739ce549eb581ca3bb13de8c3aa164657235814e3c1edb2a050fde5dbf24f2" => :sierra
  end

  depends_on "openssl@1.1"

  def install
    ENV.append "LIBS", "-liconv"
    system "./configure", "--prefix=#{prefix}",
                          "--with-default-server=irc.freenode.net",
                          "--enable-ipv6"
    system "make"
    ENV.deparallelize
    system "make", "install"
  end

  test do
    IO.popen("#{bin}/irc -d", "r+") do |pipe|
      assert_match "Connecting to port 6667 of server irc.freenode.net", pipe.gets
      pipe.puts "/quit"
      pipe.close_write
      pipe.close
    end
  end
end
