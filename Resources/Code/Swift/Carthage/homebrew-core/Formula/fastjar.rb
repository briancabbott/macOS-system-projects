class Fastjar < Formula
  desc "Implementation of Sun's jar tool"
  homepage "https://savannah.nongnu.org/projects/fastjar"
  url "https://download.savannah.nongnu.org/releases/fastjar/fastjar-0.98.tar.gz"
  sha256 "f156abc5de8658f22ee8f08d7a72c88f9409ebd8c7933e9466b0842afeb2f145"
  license "GPL-2.0"

  livecheck do
    url "https://download.savannah.nongnu.org/releases/fastjar/"
    regex(/href=.*?fastjar[._-]v?(\d+(?:\.\d+)+)\.t/i)
  end

  bottle do
    cellar :any_skip_relocation
    sha256 "1cbf953373b3d48accd872aa603fa66fb3b96e1ec33d4a17dfe6da2f97ad5cbb" => :big_sur
    sha256 "d9bb50e8b725164441e747625b381631edf82e0040babda6f187466295f80e3f" => :arm64_big_sur
    sha256 "ee758c76cb694c96ea30cb9e6ac204f2797c78be36610dcdf36c2a75301b5835" => :catalina
    sha256 "2dba61ec801db3d83692b9c9dd26eab247cb4bb6a6d6afc27f059bb6ba6052e5" => :mojave
    sha256 "87b2c870895191b309b595481f73346e763b87c661e64ef35b821e54395d5cc1" => :high_sierra
    sha256 "35230e788987e3a3c63d126af24c634bcbf58c0a320223d61f0eae69f6cbcc00" => :sierra
  end

  def install
    system "./configure", "--disable-debug", "--disable-dependency-tracking",
                          "--prefix=#{prefix}"
    system "make", "install"
  end

  test do
    system "#{bin}/fastjar", "-V"
    system "#{bin}/grepjar", "-V"
  end
end
