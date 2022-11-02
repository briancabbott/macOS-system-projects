class Streamripper < Formula
  desc "Separate tracks via Shoutcasts title-streaming"
  homepage "https://streamripper.sourceforge.io/"
  url "https://downloads.sourceforge.net/project/streamripper/streamripper%20%28current%29/1.64.6/streamripper-1.64.6.tar.gz"
  sha256 "c1d75f2e9c7b38fd4695be66eff4533395248132f3cc61f375196403c4d8de42"
  revision 1

  livecheck do
    url :stable
    regex(%r{url=.*?/streamripper[._-]v?(\d+(?:\.\d+)+)\.t}i)
  end

  bottle do
    cellar :any
    sha256 "95c5e14ced5421594802535f00d84b763a92c3cf20cf399a5bfdfa77b68b48ab" => :big_sur
    sha256 "bdc01265cc82de8fdd17a432458a22ea22420839daed5d29234efe5c9cf459a2" => :catalina
    sha256 "559e6ce06f450c306178c1e361154f134c3478ad1bc35ca70d0d3f000938043d" => :mojave
    sha256 "9df7827f89ef7f517ccfdb52be976b358ede1ceb2690f8617b4cc52da7c4cf41" => :high_sierra
    sha256 "2ccd049ca0ce6720055a86b726bfb1388b4e3784b2cd597bc7b26fd1e593a60c" => :sierra
  end

  depends_on "pkg-config" => :build
  depends_on "glib"

  def install
    # the Makefile ignores CPPFLAGS from the environment, which
    # breaks the build when HOMEBREW_PREFIX is not /usr/local
    ENV.append_to_cflags ENV.cppflags

    chmod 0755, "./install-sh" # or "make install" fails

    system "./configure", "--prefix=#{prefix}",
                          "--disable-debug",
                          "--disable-dependency-tracking"
    system "make", "install"
  end

  test do
    system "#{bin}/streamripper", "--version"
  end
end
