class Rancid < Formula
  desc "Really Awesome New Cisco confIg Differ"
  homepage "https://www.shrubbery.net/rancid/"
  url "https://www.shrubbery.net/pub/rancid/rancid-3.13.tar.gz"
  mirror "https://deb.debian.org/debian/pool/main/r/rancid/rancid_3.13.orig.tar.gz"
  sha256 "7241d2972b1f6f76a28bdaa0e7942b1257e08b404a15d121c9dee568178f8bf5"

  bottle do
    cellar :any_skip_relocation
    sha256 "3c22c8b4feebcaf1b03f4feb919c352d2b449aab6341b1fe81164fa771240826" => :big_sur
    sha256 "b815068fba2453ad568c0406b1f8bd1b1dfe6c69891ac1301a57b01934141132" => :arm64_big_sur
    sha256 "6840b7e2cb719007f53317491e8fe88a56820c121d52ff2bda4403bbcd0ea151" => :catalina
    sha256 "28b5457df20fc95e94e12925073469ba25d31924e622bfca882721fc2852dba7" => :mojave
    sha256 "3f2863b14389c488ace412c10ac68fc82dd01d6d26457c356f58d7de7c7d2d0a" => :high_sierra
  end

  conflicts_with "par", because: "both install `par` binaries"

  def install
    system "./configure", "--prefix=#{prefix}", "--exec-prefix=#{prefix}", "--mandir=#{man}"
    system "make", "install"
  end

  test do
    (testpath/"rancid.conf").write <<~EOS
      BASEDIR=#{testpath}; export BASEDIR
      CVSROOT=$BASEDIR/CVS; export CVSROOT
      LOGDIR=$BASEDIR/logs; export LOGDIR
      RCSSYS=git; export RCSSYS
      LIST_OF_GROUPS="backbone aggregation switches"
    EOS
    system "#{bin}/rancid-cvs", "-f", testpath/"rancid.conf"
  end
end
