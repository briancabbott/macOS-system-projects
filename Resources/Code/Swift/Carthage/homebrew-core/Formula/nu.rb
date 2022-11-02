class Nu < Formula
  desc "Object-oriented, Lisp-like programming language"
  homepage "https://programming.nu/"
  url "https://github.com/nulang/nu/archive/v2.3.0.tar.gz"
  sha256 "1a6839c1f45aff10797dd4ce5498edaf2f04c415b3c28cd06a7e0697d6133342"
  license "Apache-2.0"

  bottle do
    cellar :any
    sha256 "f99e9ccd7919c4e2058299e3c545c26ac2fca23a241550fd306afcee6c790d98" => :big_sur
    sha256 "d785730e9226dbfe78513a268657bfa50bacd5427b8779f838d00f1c312cc2a8" => :catalina
    sha256 "a3e605c8fca139258b5b5d49f85ac4d57a781017ae0deac8096a74d491219121" => :mojave
    sha256 "119f4f3eed1bf677c4e8d0248bd4d042d6c7333d21e6442b90440504bb2e276a" => :high_sierra
  end

  depends_on "pcre"

  def install
    ENV.delete("SDKROOT") if MacOS.version < :sierra
    ENV["PREFIX"] = prefix

    inreplace "Nukefile" do |s|
      s.gsub!('(SH "sudo ', '(SH "') # don't use sudo to install
      s.gsub!("\#{@destdir}/Library/Frameworks", "\#{@prefix}/Frameworks")
      s.sub! /^;; source files$/, <<~EOS
        ;; source files
        (set @framework_install_path "#{frameworks}")
      EOS
    end
    system "make"
    system "./mininush", "tools/nuke"
    bin.mkdir
    lib.mkdir
    include.mkdir
    system "./mininush", "tools/nuke", "install"
  end

  def caveats
    <<~EOS
      Nu.framework was installed to:
        #{frameworks}/Nu.framework

      You may want to symlink this Framework to a standard macOS location,
      such as:
        ln -s "#{frameworks}/Nu.framework" /Library/Frameworks
    EOS
  end

  test do
    system bin/"nush", "-e", '(puts "Everything old is Nu again.")'
  end
end
