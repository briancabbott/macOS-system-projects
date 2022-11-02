class GccAT5 < Formula
  desc "GNU Compiler Collection"
  homepage "https://gcc.gnu.org/"
  url "https://ftp.gnu.org/gnu/gcc/gcc-5.5.0/gcc-5.5.0.tar.xz"
  mirror "https://ftpmirror.gnu.org/gcc/gcc-5.5.0/gcc-5.5.0.tar.xz"
  sha256 "530cea139d82fe542b358961130c69cfde8b3d14556370b65823d2f91f0ced87"
  revision 6

  livecheck do
    url :stable
    regex(%r{href=.*?gcc[._-]v?(5(?:\.\d+)+)(?:/?["' >]|\.t)}i)
  end

  bottle do
    sha256 "dcc9059b725fd7c87842287bbedf60a28745417652d42a300dcd944e15986f36" => :high_sierra
  end

  # The bottles are built on systems with the CLT installed, and do not work
  # out of the box on Xcode-only systems due to an incorrect sysroot.
  pour_bottle? do
    reason "The bottle needs the Xcode CLT to be installed."
    satisfy { MacOS::CLT.installed? }
  end

  depends_on maximum_macos: [:high_sierra, :build]

  depends_on "gmp"
  depends_on "isl@0.18"
  depends_on "libmpc"
  depends_on "mpfr"

  uses_from_macos "zlib"

  # GCC bootstraps itself, so it is OK to have an incompatible C++ stdlib
  cxxstdlib_check :skip

  # Fix build with Xcode 9
  # https://gcc.gnu.org/bugzilla/show_bug.cgi?id=82091
  if DevelopmentTools.clang_build_version >= 900
    patch do
      url "https://raw.githubusercontent.com/Homebrew/formula-patches/078797f1b9/gcc%405/xcode9.patch"
      sha256 "e1546823630c516679371856338abcbab381efaf9bd99511ceedcce3cf7c0199"
    end
  end

  # Fix Apple headers, otherwise they trigger a build failure in libsanitizer
  # GCC bug report: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=83531
  # Apple radar 36176941
  if MacOS.version == :high_sierra
    patch do
      url "https://raw.githubusercontent.com/Homebrew/formula-patches/413cfac6/gcc%405/10.13_headers.patch"
      sha256 "94aaec20c8c7bfd3c41ef8fb7725bd524b1c0392d11a411742303a3465d18d09"
    end
  end

  # Patch for Xcode bug, taken from https://gcc.gnu.org/bugzilla/show_bug.cgi?id=89864#c43
  # This should be removed in the next release of GCC if fixed by apple; this is an xcode bug,
  # but this patch is a work around committed to GCC trunk
  if MacOS::Xcode.version >= "10.2"
    patch do
      url "https://raw.githubusercontent.com/Homebrew/formula-patches/91d57ebe88e17255965fa88b53541335ef16f64a/gcc%405/gcc5-xcode10.2.patch"
      sha256 "6834bec30c54ab1cae645679e908713102f376ea0fc2ee993b3c19995832fe56"
    end
  end

  def install
    # GCC will suffer build errors if forced to use a particular linker.
    ENV.delete "LD"

    # C, C++, ObjC and Fortran compilers are always built
    languages = %w[c c++ fortran objc obj-c++]

    version_suffix = version.major.to_s

    # Even when suffixes are appended, the info pages conflict when
    # install-info is run so pretend we have an outdated makeinfo
    # to prevent their build.
    ENV["gcc_cv_prog_makeinfo_modern"] = "no"

    args = [
      "--build=x86_64-apple-darwin#{OS.kernel_version}",
      "--prefix=#{prefix}",
      "--libdir=#{lib}/gcc/#{version_suffix}",
      "--enable-languages=#{languages.join(",")}",
      # Make most executables versioned to avoid conflicts.
      "--program-suffix=-#{version_suffix}",
      "--with-gmp=#{Formula["gmp"].opt_prefix}",
      "--with-mpfr=#{Formula["mpfr"].opt_prefix}",
      "--with-mpc=#{Formula["libmpc"].opt_prefix}",
      "--with-isl=#{Formula["isl@0.18"].opt_prefix}",
      "--with-system-zlib",
      "--enable-libstdcxx-time=yes",
      "--enable-stage1-checking",
      "--enable-checking=release",
      "--enable-lto",
      "--enable-plugin",
      # A no-op unless --HEAD is built because in head warnings will
      # raise errors. But still a good idea to include.
      "--disable-werror",
      "--disable-nls",
      "--with-pkgversion=Homebrew GCC #{pkg_version} #{build.used_options*" "}".strip,
      "--with-bugurl=https://github.com/Homebrew/homebrew-core/issues",
      "--enable-multilib",
    ]

    # System headers may not be in /usr/include
    sdk = MacOS.sdk_path_if_needed
    if sdk
      args << "--with-native-system-header-dir=/usr/include"
      args << "--with-sysroot=#{sdk}"
    end

    # Avoid reference to sed shim
    args << "SED=/usr/bin/sed"

    # Ensure correct install names when linking against libgcc_s;
    # see discussion in https://github.com/Homebrew/homebrew/pull/34303
    inreplace "libgcc/config/t-slibgcc-darwin", "@shlib_slibdir@", "#{HOMEBREW_PREFIX}/lib/gcc/#{version_suffix}"

    mkdir "build" do
      system "../configure", *args
      system "make", "bootstrap"
      system "make", "install"
    end

    # Handle conflicts between GCC formulae.
    # Rename man7.
    Dir.glob(man7/"*.7") { |file| add_suffix file, version_suffix }
    # Even when we disable building info pages some are still installed.
    info.rmtree
  end

  def add_suffix(file, suffix)
    dir = File.dirname(file)
    ext = File.extname(file)
    base = File.basename(file, ext)
    File.rename file, "#{dir}/#{base}-#{suffix}#{ext}"
  end

  test do
    (testpath/"hello-c.c").write <<~EOS
      #include <stdio.h>
      int main()
      {
        puts("Hello, world!");
        return 0;
      }
    EOS
    system bin/"gcc-#{version.major}", "-o", "hello-c", "hello-c.c"
    assert_equal "Hello, world!\n", `./hello-c`
  end
end
