class Latex2rtf < Formula
  desc "Translate LaTeX to RTF"
  homepage "https://latex2rtf.sourceforge.io/"
  url "https://downloads.sourceforge.net/project/latex2rtf/latex2rtf-unix/2.3.18/latex2rtf-2.3.18a.tar.gz"
  sha256 "338ba2e83360f41ded96a0ceb132db9beaaf15018b36101be2bae8bb239017d9"
  license "GPL-2.0-or-later"

  livecheck do
    url :stable
    regex(%r{url=.*?/latex2rtf/files/latex2rtf-unix/[^/]+/latex2rtf[._-](\d+(?:[-.]\d+)+[a-z]?)\.t}i)
  end

  bottle do
    sha256 "fedf28c8cd7113f639a32776b9b55bbbae3ccfa7aa15e142d08004d39cf56d23" => :big_sur
    sha256 "29b2cd9987d2362995534aed209cf84ff93cb307de474bbe2ff16c5e94bfc9cb" => :arm64_big_sur
    sha256 "a4f536a8f9a6001fe955727e7d9473b5294daf416b422dab70b489067dad35f3" => :catalina
    sha256 "e57496652dd135bddb2d28f88d96e6207b69551f040ac4436cb6d043557e90c3" => :mojave
  end

  def install
    inreplace "Makefile", "cp -p doc/latex2rtf.html $(DESTDIR)$(SUPPORTDIR)",
                          "cp -p doc/web/* $(DESTDIR)$(SUPPORTDIR)"
    system "make", "DESTDIR=",
                   "BINDIR=#{bin}",
                   "MANDIR=#{man1}",
                   "INFODIR=#{info}",
                   "SUPPORTDIR=#{pkgshare}",
                   "CFGDIR=#{pkgshare}/cfg",
                   "install"
  end

  test do
    (testpath/"test.tex").write <<~EOS
      \\documentclass{article}
      \\title{LaTeX to RTF}
      \\begin{document}
      \\maketitle
      \\end{document}
    EOS
    system bin/"latex2rtf", "test.tex"
    assert_predicate testpath/"test.rtf", :exist?
    assert_match "LaTeX to RTF", File.read(testpath/"test.rtf")
  end
end
