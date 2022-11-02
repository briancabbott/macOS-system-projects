class Tcptrace < Formula
  # The tcptrace.org site has a history of going down sometimes, which is why
  # we're using mirrors even though the first-party site may be available.
  desc "Analyze tcpdump output"
  homepage "http://www.tcptrace.org/"
  url "https://www.mirrorservice.org/sites/distfiles.macports.org/tcptrace/tcptrace-6.6.7.tar.gz"
  mirror "https://distfiles.macports.org/tcptrace/tcptrace-6.6.7.tar.gz"
  sha256 "63380a4051933ca08979476a9dfc6f959308bc9f60d45255202e388eb56910bd"

  # This site has a history of going down for periods of time, which is why the
  # formula uses mirrors. This is something to be aware of if livecheck is
  # unable to find versions.
  livecheck do
    url "http://www.tcptrace.org/download.shtml"
    regex(/href=.*?tcptrace[._-]v?(\d+(?:\.\d+)+)\.t/i)
  end

  bottle do
    cellar :any_skip_relocation
    sha256 "64787cc311c9da8d2090af5732efbe42f74c6dc5037b2b7ecb7055485603f20d" => :big_sur
    sha256 "a1a61bd690da912afedd38f62eac7d5a1724c1ce68c68e7bcd8576e3fb86d956" => :catalina
    sha256 "b927868b2addc93b296fb8f31b08147014e9a81a356d4f18b0d4134db40081de" => :mojave
    sha256 "39916506fcd6385aee6375813128a126a84f947623594011f6c2c9df1b6dc8b2" => :high_sierra
    sha256 "7ccc5e6859be970a5a8a064630704111d37b03a7e3cf3a9874e16a60e4abe02b" => :sierra
    sha256 "e46775d7cc808b5b52a0a36a33142b824a9b2d8bce5b0557bc1041c2e55c5ffb" => :el_capitan
    sha256 "f9de7ef41a2b9dc8daee1fddef1035ddf6a08cf473b6edafcf4bb069ab5f0052" => :yosemite
    sha256 "03ecc0ca3c3be27ccf8bcf88be26fb03addecbd14cc1283cab7947d39f9da6ae" => :mavericks
  end

  def install
    system "./configure", "--disable-debug",
                          "--disable-dependency-tracking",
                          "--prefix=#{prefix}"
    system "make", "tcptrace"

    # don't install with owner/group
    inreplace "Makefile", "-o bin -g bin", ""
    system "make", "install", "BINDIR=#{bin}", "MANDIR=#{man}"
  end

  test do
    touch "dump"
    assert_match(/0 packets seen, 0 TCP packets/,
      shell_output("#{bin}/tcptrace dump"))
  end
end
