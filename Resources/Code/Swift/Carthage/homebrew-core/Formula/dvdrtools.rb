class Dvdrtools < Formula
  desc "Fork of cdrtools DVD writer support"
  homepage "https://savannah.nongnu.org/projects/dvdrtools/"
  url "https://savannah.nongnu.org/download/dvdrtools/dvdrtools-0.2.1.tar.gz"
  sha256 "053d0f277f69b183f9c8e8c8b09b94d5bb4a1de6d9b122c0e6c00cc6593dfb46"
  license "GPL-2.0"

  livecheck do
    url "https://download.savannah.gnu.org/releases/dvdrtools/"
    regex(/href=.*?dvdrtools[._-]v?(\d+(?:\.\d+)+)\.t/i)
  end

  bottle do
    cellar :any_skip_relocation
    sha256 "c969433ed859dab8f0551c6eab562a4cc272b063f333e0208081ea3b5940c76b" => :big_sur
    sha256 "7b93d029d54fa99b94010b18776bdf36c889e81e8f169f8745f319b7b7b6f9f0" => :arm64_big_sur
    sha256 "40f565db4f098c70bed700dc88edd45951e58a7f7c64583d52db81afcdbde704" => :catalina
    sha256 "77bee36a67611f862c4fd8fbff7b1bbc7e307f5f618508664f02193df7347865" => :mojave
    sha256 "f697f22349d9ed05ad580d06b5dc38c4b626187d50cfc364af4bb5634f16b152" => :high_sierra
    sha256 "afa198a1854643ac7657ad1c93bfc5f9b05760e3b3375dd3ec43ad0b51e4ea7e" => :sierra
    sha256 "8d29698226d26f42559f4913a13920632b85cafc389122697fa2c5c4d0cd2d8b" => :el_capitan
    sha256 "4feb2b0e87b7402706c5a382c8e35b66279aa1b73c37c7ded7a6cc14de3a8a62" => :yosemite
    sha256 "45fbfc8f888ca87b81aa7dff0f30d8cb69c36a4ce1933d76faecbd023c4ab3ad" => :mavericks
  end

  conflicts_with "cdrtools",
    because: "both cdrtools and dvdrtools install binaries by the same name"

  # Below three patches via MacPorts.
  patch :p0 do
    url "https://raw.githubusercontent.com/Homebrew/formula-patches/8a41dd4/dvdrtools/patch-cdda2wav-cdda2wav.c"
    sha256 "f792a26af38f63ee1220455da8dba2afc31296136a97c11476d8e3abe94a4a94"
  end

  patch :p0 do
    url "https://raw.githubusercontent.com/Homebrew/formula-patches/8a41dd4/dvdrtools/patch-cdrecord-cdrecord.c"
    sha256 "c7f182ce154785e19235f30d22d3cf56e60f6c9c8cc953a9d16b58205e29a039"
  end

  patch :p0 do
    url "https://raw.githubusercontent.com/Homebrew/formula-patches/8a41dd4/dvdrtools/patch-scsi-mac-iokit.c"
    sha256 "f31253e021a70cc49e026eed81c5a49166a59cb8da1a7f0695fa2f26c7a3d98f"
  end

  def install
    ENV["LIBS"] = "-framework IOKit -framework CoreFoundation"

    system "./configure", "--disable-debug",
                          "--disable-dependency-tracking",
                          "--prefix=#{prefix}",
                          "--mandir=#{man}"
    system "make", "install"
  end
end
