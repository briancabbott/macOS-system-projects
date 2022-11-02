class GoogleAuthenticatorLibpam < Formula
  desc "PAM module for two-factor authentication"
  homepage "https://github.com/google/google-authenticator-libpam"
  url "https://github.com/google/google-authenticator-libpam/archive/1.09.tar.gz"
  sha256 "ab1d7983413dc2f11de2efa903e5c326af8cb9ea37765dacb39949417f7cd037"
  license "Apache-2.0"

  bottle do
    cellar :any_skip_relocation
    sha256 "edd70a2050f2b57337558bd372f5bfa78f45df8ce678e3a6c400310edaa830a9" => :big_sur
    sha256 "8adf8be0fbea5003e02b748ef71a099635eb8dff716c62d6935581d493fcda78" => :arm64_big_sur
    sha256 "4ed85644559250923d4b21f5b99643cad08eb8bbb63afc3827d7ac225b4581d7" => :catalina
    sha256 "d62c1f21ec88406788b314bd7a06c0e37e7ab9dad4237f6832441f235723d3cb" => :mojave
    sha256 "33fa28d290cb0068a67c288d4889967180de64aa895f0ac1a3aedcc38d6a7d7a" => :high_sierra
  end

  depends_on "autoconf" => :build
  depends_on "automake" => :build
  depends_on "libtool" => :build
  depends_on "qrencode"

  def install
    system "./bootstrap.sh"
    system "./configure", "--disable-dependency-tracking",
                          "--disable-silent-rules",
                          "--prefix=#{prefix}"
    system "make", "install"
  end

  def caveats
    <<~EOS
      Add 2-factor authentication for ssh:
        echo "auth required #{opt_lib}/security/pam_google_authenticator.so" \\
        | sudo tee -a /etc/pam.d/sshd

      Add 2-factor authentication for ssh allowing users to log in without OTP:
        echo "auth required #{opt_lib}/security/pam_google_authenticator.so" \\
        "nullok" | sudo tee -a /etc/pam.d/sshd

      (Or just manually edit /etc/pam.d/sshd)
    EOS
  end

  test do
    system "#{bin}/google-authenticator", "--force", "--time-based",
           "--disallow-reuse", "--rate-limit=3", "--rate-time=30",
           "--window-size=3", "--no-confirm"
  end
end
