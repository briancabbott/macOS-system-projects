class OpenjdkAT8 < Formula
  desc "Development kit for the Java programming language"
  homepage "https://openjdk.java.net/"
  url "https://openjdk-sources.osci.io/openjdk8/openjdk8u275-ga.tar.xz"
  version "1.8.0+275"
  sha256 "6a83e8bd8caebf5dc8989a22925ac5598c9edb2ba8dcfad7472a5ff99a14c0e7"
  license "GPL-2.0-only"

  bottle do
    cellar :any
    sha256 "b37ae302878e8bdbfaaa9eb085c7315bef27c09fee834c1e43a7540b2b986d5a" => :big_sur
    sha256 "85e18d44283b8c93a7065d2dd40540278607ff6d6fdf3e460cdbf5886c051c4b" => :catalina
    sha256 "b8893a1e645893395f767b21c947ab5669093f3b60c47a3c423a358aeeb024f8" => :mojave
    sha256 "384c545a8e63a4535ed6999d046f02d25206a72d2925489cba818c63df3e2e37" => :high_sierra
  end

  keg_only :versioned_formula

  depends_on "autoconf" => :build
  depends_on "pkg-config" => :build
  depends_on "freetype"

  # Oracle doesn't serve JDK 7 downloads anymore, so use Zulu JDK 7 for bootstrapping.
  resource "boot-jdk" do
    on_macos do
      url "https://cdn.azul.com/zulu/bin/zulu7.42.0.13-ca-jdk7.0.282-macosx_x64.tar.gz"
      sha256 "37767a8ec40ff63dd43020365cf6c3e95841213cfe73aaa04ee0cffca779b2e7"
    end
    on_linux do
      url "https://cdn.azul.com/zulu/bin/zulu7.42.0.13-ca-jdk7.0.282-linux_x64.tar.gz"
      sha256 "38ec78e7f41f9130cecce5c8c9963d066f7deee5b3ba4dfcca32e197fd933bf9"
    end
  end

  # Apply this upstreamed patch to build on newer Xcode.
  # https://github.com/AdoptOpenJDK/openjdk-jdk8u/pull/10
  patch do
    url "https://raw.githubusercontent.com/Homebrew/formula-patches/9976a857d574de2927c580f1f61bcd647fb795fe/openjdk%408/xcode.patch"
    sha256 "f59a82f2e83c97a7496ba71c811ee0849d7df6b45e32fb3da0f0078386eebd80"
  end

  def install
    _, _, update = version.to_s.rpartition("+")
    java_options = ENV.delete("_JAVA_OPTIONS")

    boot_jdk = buildpath/"boot-jdk"
    resource("boot-jdk").stage(boot_jdk)

    # Work around clashing -I/usr/include and -isystem headers,
    # as superenv already handles this detail for us.
    inreplace "common/autoconf/flags.m4",
              '-isysroot \"$SYSROOT\"', ""
    inreplace "common/autoconf/toolchain.m4",
              '-isysroot \"$SDKPATH\" -iframework\"$SDKPATH/System/Library/Frameworks\"', ""
    inreplace "hotspot/make/bsd/makefiles/saproc.make",
              '-isysroot "$(SDKPATH)" -iframework"$(SDKPATH)/System/Library/Frameworks"', ""

    # Fix macOS version detection. After 10.10 this was changed to a 6 digit number,
    # but this Makefile was written in the era of 4 digit numbers.
    inreplace "hotspot/make/bsd/makefiles/gcc.make" do |s|
      s.gsub! "$(subst .,,$(MACOSX_VERSION_MIN))", ENV["HOMEBREW_MACOS_VERSION_NUMERIC"]
      s.gsub! "MACOSX_VERSION_MIN=10.7.0", "MACOSX_VERSION_MIN=#{MacOS.version}"
    end

    # Fix to permit building with Xcode 12
    inreplace "common/autoconf/toolchain.m4",
              '"${XC_VERSION_PARTS[[0]]}" != "4"',
              '"${XC_VERSION_PARTS[[0]]}" != "12"'

    args = %W[--with-boot-jdk-jvmargs=#{java_options}
              --with-boot-jdk=#{boot_jdk}
              --with-debug-level=release
              --with-jvm-variants=server
              --with-milestone=fcs
              --with-native-debug-symbols=none
              --with-toolchain-type=clang
              --with-update-version=#{update}]

    # Work around SDK issues with JavaVM framework.
    if MacOS.version <= :catalina
      sdk_path = MacOS::CLT.sdk_path(MacOS.version)
      ENV["SDKPATH"] = ENV["SDKROOT"] = sdk_path
      javavm_framework_path = sdk_path/"System/Library/Frameworks/JavaVM.framework/Frameworks"
      args += %W[--with-extra-cflags=-F#{javavm_framework_path}
                 --with-extra-cxxflags=-F#{javavm_framework_path}
                 --with-extra-ldflags=-F#{javavm_framework_path}]
    end

    chmod 0755, %w[configure common/autoconf/autogen.sh]

    system "common/autoconf/autogen.sh"
    system "./configure", *args

    ENV["MAKEFLAGS"] = "JOBS=#{ENV.make_jobs}"
    system "make", "images"

    jdk = Dir["build/*/images/j2sdk-bundle/*"].first
    libexec.install jdk => "openjdk.jdk"
    bin.install_symlink Dir["#{libexec}/openjdk.jdk/Contents/Home/bin/*"]
    include.install_symlink Dir["#{libexec}/openjdk.jdk/Contents/Home/include/*.h"]
    include.install_symlink Dir["#{libexec}/openjdk.jdk/Contents/Home/include/darwin/*.h"]
  end

  def caveats
    <<~EOS
      For the system Java wrappers to find this JDK, symlink it with
        sudo ln -sfn #{opt_libexec}/openjdk.jdk /Library/Java/JavaVirtualMachines/openjdk-8.jdk
    EOS
  end

  test do
    (testpath/"HelloWorld.java").write <<~EOS
      class HelloWorld {
        public static void main(String args[]) {
          System.out.println("Hello, world!");
        }
      }
    EOS

    system bin/"javac", "HelloWorld.java"

    assert_match "Hello, world!", shell_output("#{bin}/java HelloWorld")
  end
end
