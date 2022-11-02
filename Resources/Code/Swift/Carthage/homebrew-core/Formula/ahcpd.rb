class Ahcpd < Formula
  desc "Autoconfiguration protocol for IPv6 and IPv6/IPv4 networks"
  homepage "https://www.irif.univ-paris-diderot.fr/~jch/software/ahcp/"
  url "https://www.irif.univ-paris-diderot.fr/~jch/software/files/ahcpd-0.53.tar.gz"
  sha256 "a4622e817d2b2a9b878653f085585bd57f3838cc546cca6028d3b73ffcac0d52"
  license "MIT"

  bottle do
    cellar :any_skip_relocation
    sha256 "d61fe30a858cd3579dbcc065e0c13b18b39669ae8a94497c66fdde6796396088" => :big_sur
    sha256 "a11c4af36a9b0bce92a21e77c69421a4dbbd30b377b3fd0e7f2cc97f4e02c677" => :arm64_big_sur
    sha256 "7440676cc30eed4de9c2cb3ad3c9a9691ba0da6636e4d38a33722ca54d168c9d" => :catalina
    sha256 "8852e7e5e11d6ea413657d012e4d49ca0d9ac406e56da6bf7c0daa6d4d788a16" => :mojave
    sha256 "ab3221a9f28ded916f8d2ef4b8377a2a793fa2fee5f891b9a97e3dede0d294ae" => :high_sierra
    sha256 "d3a8a4efb712e2c6a8a055276e5d93d3275a638df4231a4dfe8d428a2606d776" => :sierra
    sha256 "b37143ee365a4a3afd9623d5f49eab0bc4bdf9ac3662d22db9671cffa1078224" => :el_capitan
    sha256 "36907bc1aadc9d9d874ebd74624d8c2c2e8b4057181df1e964720a41f72ccae8" => :yosemite
    sha256 "8518f82187d2b8d2bc24648bd072f19073e159abb2bdaf5418ad31e3ab966d0b" => :mavericks
  end

  patch :DATA

  def install
    system "make", "LDLIBS=''"
    system "make", "install", "PREFIX=", "TARGET=#{prefix}"
  end

  test do
    pid_file = testpath/"ahcpd.pid"
    log_file = testpath/"ahcpd.log"
    mkdir testpath/"leases"

    (testpath/"ahcpd.conf").write <<~EOS
      mode server

      prefix fde6:20f5:c9ac:358::/64
      prefix 192.168.4.128/25
      lease-dir #{testpath}/leases
      name-server fde6:20f5:c9ac:358::1
      name-server 192.168.4.1
      ntp-server 192.168.4.2
    EOS

    system "#{bin}/ahcpd", "-c", "ahcpd.conf", "-I", pid_file, "-L", log_file, "-D", "lo0"
    sleep(2)

    assert_predicate pid_file, :exist?, "The file containing the PID of the child process was not created."
    assert_predicate log_file, :exist?, "The file containing the log was not created."

    Process.kill("TERM", pid_file.read.to_i)
  end
end

__END__
diff --git a/Makefile b/Makefile
index e52eeb7..28e1043 100644
--- a/Makefile
+++ b/Makefile
@@ -40,8 +40,8 @@ install.minimal: all
	chmod +x $(TARGET)/etc/ahcp/ahcp-config.sh

 install: all install.minimal
-	mkdir -p $(TARGET)$(PREFIX)/man/man8/
-	cp -f ahcpd.man $(TARGET)$(PREFIX)/man/man8/ahcpd.8
+	mkdir -p $(TARGET)$(PREFIX)/share/man/man8/
+	cp -f ahcpd.man $(TARGET)$(PREFIX)/share/man/man8/ahcpd.8

 .PHONY: uninstall

@@ -49,7 +49,7 @@ uninstall:
	-rm -f $(TARGET)$(PREFIX)/bin/ahcpd
	-rm -f $(TARGET)$(PREFIX)/bin/ahcp-config.sh
	-rm -f $(TARGET)$(PREFIX)/bin/ahcp-dummy-config.sh
-	-rm -f $(TARGET)$(PREFIX)/man/man8/ahcpd.8
+	-rm -f $(TARGET)$(PREFIX)/share/man/man8/ahcpd.8

 .PHONY: clean
