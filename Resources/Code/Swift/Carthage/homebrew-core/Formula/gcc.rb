class Gcc < Formula
  desc "GNU compiler collection"
  homepage "https://gcc.gnu.org/"
  if Hardware::CPU.arm?
    # Branch from the Darwin maintainer of GCC with Apple Silicon support,
    # located at https://github.com/iains/gcc-darwin-arm64 and
    # backported with his help to gcc-10 branch. Too big for a patch.
    url "https://github.com/fxcoudert/gcc/archive/gcc-10-arm-20201228.tar.gz"
    sha256 "dd5377a13f0ee4645bce1c18ed7327ea4ad5f8bd5c6a2a24eb299c647d3d43f4"
    version "10.2.0"
  else
    url "https://ftp.gnu.org/gnu/gcc/gcc-10.2.0/gcc-10.2.0.tar.xz"
    mirror "https://ftpmirror.gnu.org/gcc/gcc-10.2.0/gcc-10.2.0.tar.xz"
    sha256 "b8dd4368bb9c7f0b98188317ee0254dd8cc99d1e3a18d0ff146c855fe16c1d8c"
  end
  license "GPL-3.0"
  revision 2
  head "https://gcc.gnu.org/git/gcc.git"

  livecheck do
    # Should be
    # url :stable
    # but that does not work with the ARM-specific branch above
    url "https://ftp.gnu.org/gnu/gcc/gcc-10.2.0"
    regex(%r{href=.*?gcc[._-]v?(\d+(?:\.\d+)+)(?:/?["' >]|\.t)}i)
  end

  bottle do
    sha256 "7d6952bb9c36d08e658e7e9b0637805a49184a1c7506c020ffa839e7ca756ba3" => :big_sur
    sha256 "537582c8fb519a87b3e213953a8c2daadf11339559b9da17dd9fab95d8768dfc" => :arm64_big_sur
    sha256 "4c17591306a2c233bdb9c469ceec3057a1b76ac4d194942737c322ffc22cb440" => :catalina
    sha256 "d4ef938e9e1d7e4247b653e4b3f5fb13e55a3dc6d50ea64f0e914271edd78c7c" => :mojave
  end

  # The bottles are built on systems with the CLT installed, and do not work
  # out of the box on Xcode-only systems due to an incorrect sysroot.
  pour_bottle? do
    reason "The bottle needs the Xcode CLT to be installed."
    satisfy { MacOS::CLT.installed? }
  end

  depends_on "gmp"
  depends_on "isl"
  depends_on "libmpc"
  depends_on "mpfr"

  uses_from_macos "zlib"

  # GCC bootstraps itself, so it is OK to have an incompatible C++ stdlib
  cxxstdlib_check :skip

  if Hardware::CPU.arm?
    # Patch for Big Sur version numbering, remove with GCC 11
    # https://github.com/iains/gcc-darwin-arm64/commit/556ab512
    patch do
      url "https://raw.githubusercontent.com/Homebrew/formula-patches/7baf6e2f/gcc/bigsur.diff"
      sha256 "42de3bc4889b303258a4075f88ad8624ea19384cab57a98a5270638654b83f41"
    end
  end

  def version_suffix
    if build.head?
      "HEAD"
    else
      version.major.to_s
    end
  end

  def install
    # GCC will suffer build errors if forced to use a particular linker.
    ENV.delete "LD"

    # We avoiding building:
    #  - Ada, which requires a pre-existing GCC Ada compiler to bootstrap
    #  - Go, currently not supported on macOS
    #  - BRIG
    languages = %w[c c++ objc obj-c++ fortran]

    pkgversion = "Homebrew GCC #{pkg_version} #{build.used_options*" "}".strip
    cpu = Hardware::CPU.arm? ? "aarch64" : "x86_64"

    args = %W[
      --build=#{cpu}-apple-darwin#{OS.kernel_version.major}
      --prefix=#{prefix}
      --libdir=#{lib}/gcc/#{version_suffix}
      --disable-nls
      --enable-checking=release
      --enable-languages=#{languages.join(",")}
      --program-suffix=-#{version_suffix}
      --with-gmp=#{Formula["gmp"].opt_prefix}
      --with-mpfr=#{Formula["mpfr"].opt_prefix}
      --with-mpc=#{Formula["libmpc"].opt_prefix}
      --with-isl=#{Formula["isl"].opt_prefix}
      --with-system-zlib
      --with-pkgversion=#{pkgversion}
      --with-bugurl=https://github.com/Homebrew/homebrew-core/issues
    ]

    # Xcode 10 dropped 32-bit support
    args << "--disable-multilib" if DevelopmentTools.clang_build_version >= 1000

    # System headers may not be in /usr/include
    sdk = MacOS.sdk_path_if_needed
    if sdk
      args << "--with-native-system-header-dir=/usr/include"
      args << "--with-sysroot=#{sdk}"
    end

    # Mojave uses the Catalina SDK which causes issues like
    # https://github.com/Homebrew/homebrew-core/issues/46393
    ENV["ac_cv_func_aligned_alloc"] = "no" if MacOS.version == :mojave

    # Avoid reference to sed shim
    args << "SED=/usr/bin/sed"

    # Ensure correct install names when linking against libgcc_s;
    # see discussion in https://github.com/Homebrew/legacy-homebrew/pull/34303
    inreplace "libgcc/config/t-slibgcc-darwin", "@shlib_slibdir@", "#{HOMEBREW_PREFIX}/lib/gcc/#{version_suffix}"

    mkdir "build" do
      system "../configure", *args

      # Use -headerpad_max_install_names in the build,
      # otherwise updated load commands won't fit in the Mach-O header.
      # This is needed because `gcc` avoids the superenv shim.
      system "make", "BOOT_LDFLAGS=-Wl,-headerpad_max_install_names"
      system "make", "install"

      bin.install_symlink bin/"gfortran-#{version_suffix}" => "gfortran"
    end

    # Handle conflicts between GCC formulae and avoid interfering
    # with system compilers.
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
    system "#{bin}/gcc-#{version_suffix}", "-o", "hello-c", "hello-c.c"
    assert_equal "Hello, world!\n", `./hello-c`

    (testpath/"hello-cc.cc").write <<~EOS
      #include <iostream>
      int main()
      {
        std::cout << "Hello, world!" << std::endl;
        return 0;
      }
    EOS
    system "#{bin}/g++-#{version_suffix}", "-o", "hello-cc", "hello-cc.cc"
    assert_equal "Hello, world!\n", `./hello-cc`

    (testpath/"test.f90").write <<~EOS
      integer,parameter::m=10000
      real::a(m), b(m)
      real::fact=0.5

      do concurrent (i=1:m)
        a(i) = a(i) + fact*b(i)
      end do
      write(*,"(A)") "Done"
      end
    EOS
    system "#{bin}/gfortran", "-o", "test", "test.f90"
    assert_equal "Done\n", `./test`
  end
end
