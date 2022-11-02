class Hive < Formula
  desc "Hadoop-based data summarization, query, and analysis"
  homepage "https://hive.apache.org"
  url "https://www.apache.org/dyn/closer.lua?path=hive/hive-3.1.2/apache-hive-3.1.2-bin.tar.gz"
  mirror "https://archive.apache.org/dist/hive/hive-3.1.2/apache-hive-3.1.2-bin.tar.gz"
  sha256 "d75dcf36908b4e7b9b0ec9aec57a46a6628b97b276c233cb2c2f1a3e89b13462"
  license "Apache-2.0"
  revision 2

  livecheck do
    url :stable
  end

  bottle :unneeded

  depends_on "hadoop"
  depends_on "openjdk"

  def install
    rm_f Dir["bin/*.cmd", "bin/ext/*.cmd", "bin/ext/util/*.cmd"]
    libexec.install %w[bin conf examples hcatalog lib scripts]

    # Hadoop currently supplies a newer version
    # and two versions on the classpath causes problems
    rm libexec/"lib/guava-19.0.jar"
    guava = (Formula["hadoop"].opt_libexec/"share/hadoop/common/lib").glob("guava-*-jre.jar")
    ln_s guava.first, libexec/"lib"

    Pathname.glob("#{libexec}/bin/*") do |file|
      next if file.directory?

      (bin/file.basename).write_env_script file,
        Language::Java.java_home_env.merge(HIVE_HOME: libexec)
    end
  end

  def caveats
    <<~EOS
      Hadoop must be in your path for hive executable to work.

      If you want to use HCatalog with Pig, set $HCAT_HOME in your profile:
        export HCAT_HOME=#{opt_libexec}/hcatalog
    EOS
  end

  test do
    system bin/"schematool", "-initSchema", "-dbType", "derby"
    assert_match "Hive #{version}", shell_output("#{bin}/hive --version")
  end
end
