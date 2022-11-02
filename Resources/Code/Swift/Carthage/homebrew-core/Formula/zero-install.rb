class ZeroInstall < Formula
  desc "Decentralised cross-platform software installation system"
  homepage "https://0install.net/"
  url "https://github.com/0install/0install.git",
      tag:      "v2.17",
      revision: "4a837bd638d93905b96d073c28c644894f8d4a0b"
  license "LGPL-2.1-or-later"
  head "https://github.com/0install/0install.git"

  livecheck do
    url :head
    regex(/^v?(\d+(?:\.\d+)+)$/i)
  end

  bottle do
    cellar :any_skip_relocation
    sha256 "77fe4b65401743e8cd82de23568ad9e630e50467018faba2b7167231fe14f48a" => :big_sur
    sha256 "4306ae5d0ca339a7f5ecd9c7ba6a3a192a1d176883d49dda9d31aad78bc390fd" => :catalina
    sha256 "73b04cd9560f78c799599fc4f9fba0de2b072c56e2195ef0522bb23e6eeb376b" => :mojave
    sha256 "4fb5867d432bd3e22525b95682521a12a3279dd4fb7f8b0df3cb6664a6959835" => :high_sierra
  end

  depends_on "ocaml" => :build
  depends_on "ocamlbuild" => :build
  depends_on "opam" => :build
  depends_on "pkg-config" => :build
  depends_on "gnupg"

  def install
    ENV.append_path "PATH", Formula["gnupg"].opt_bin

    # Use correct curl headers
    ENV["HOMEBREW_SDKROOT"] = MacOS::CLT.sdk_path(MacOS.version) if MacOS.version >= :mojave && MacOS::CLT.installed?

    Dir.mktmpdir("opamroot") do |opamroot|
      ENV["OPAMROOT"] = opamroot
      ENV["OPAMYES"] = "1"
      ENV["OPAMVERBOSE"] = "1"
      system "opam", "init", "--no-setup", "--disable-sandboxing"
      modules = %w[
        yojson
        xmlm
        ounit
        lwt_react
        ocurl
        sha
        dune
      ]
      system "opam", "config", "exec", "opam", "install", *modules

      # mkdir: <buildpath>/build: File exists.
      # https://github.com/0install/0install/issues/87
      ENV.deparallelize { system "opam", "config", "exec", "make" }

      inreplace "dist/install.sh" do |s|
        s.gsub! '"/usr/local"', prefix
        s.gsub! '"${PREFIX}/man"', man
      end
      system "make", "install"
    end
  end

  test do
    (testpath/"hello.py").write <<~EOS
      print("hello world")
    EOS
    (testpath/"hello.xml").write <<~EOS
      <?xml version="1.0" ?>
      <interface xmlns="http://zero-install.sourceforge.net/2004/injector/interface">
        <name>Hello</name>
        <summary>minimal demonstration program</summary>

        <implementation id="." version="0.1-pre">
          <command name='run' path='hello.py'>
            <runner interface='http://repo.roscidus.com/python/python'></runner>
          </command>
        </implementation>
      </interface>
    EOS
    assert_equal "hello world\n", shell_output("#{bin}/0launch --console hello.xml")
  end
end
