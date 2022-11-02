class CheckPostgres < Formula
  desc "Monitor Postgres databases"
  homepage "https://bucardo.org/wiki/Check_postgres"
  url "https://bucardo.org/downloads/check_postgres-2.25.0.tar.gz"
  sha256 "11b52f86c44d6cc26e9a4129e67c2589071dbe1b8ac1f8895761517491c6e44b"
  license "BSD-2-Clause"
  revision 1
  head "https://github.com/bucardo/check_postgres.git"

  livecheck do
    url "https://bucardo.org/check_postgres/"
    regex(/latest version.*?v?(\d+(?:\.\d+)+)/i)
  end

  bottle do
    cellar :any_skip_relocation
    sha256 "4d3fab04056d9f73cbbf1687301e43993c696637d86146d4124b74a0222d321f" => :big_sur
    sha256 "1b4c6b85f790396a5c498e7354b548e981fcdc84d9c0fef2cfee0cbf6a8111de" => :arm64_big_sur
    sha256 "6a52850ba011c00b1daf005009ef0143d02d397cd2b212f69ffcc92f9c93e7a5" => :catalina
    sha256 "09f45361f23beae689194d98e4a8d4788e38d8a57f8be94c6fb5bcc5a7dd8950" => :mojave
  end

  depends_on "postgresql"

  def install
    system "perl", "Makefile.PL", "INSTALL_BASE=#{prefix}", "INSTALLSITEMAN1DIR=#{man1}"
    system "make", "install"
    mv bin/"check_postgres.pl", bin/"check_postgres"
    inreplace [bin/"check_postgres", man1/"check_postgres.1p"], "check_postgres.pl", "check_postgres"
    rm_rf prefix/"Library"
    rm_rf prefix/"lib"
  end

  test do
    # This test verifies that check_postgres fails correctly, assuming
    # that no server is running at that port.
    output = shell_output("#{bin}/check_postgres --action=connection --port=65432", 2)
    assert_match /POSTGRES_CONNECTION CRITICAL/, output
  end
end
