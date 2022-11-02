class Clojurescript < Formula
  desc "Clojure to JS compiler"
  homepage "https://github.com/clojure/clojurescript"
  url "https://github.com/clojure/clojurescript/releases/download/r1.10.758/cljs.jar"
  sha256 "dcc98e103d281d4eab21ca94fba11728e9f587c3aa09c8ffc3b96cff210adcce"
  license "EPL-1.0"
  revision 1
  head "https://github.com/clojure/clojurescript.git"

  livecheck do
    url :stable
    strategy :github_latest
    regex(%r{href=.*?/tag/r?(\d+(?:\.\d+)+)["' >]}i)
  end

  bottle :unneeded

  depends_on "openjdk"

  def install
    libexec.install "cljs.jar"
    bin.write_jar_script libexec/"cljs.jar", "cljsc"
  end

  def caveats
    <<~EOS
      This formula is useful if you need to use the ClojureScript compiler directly.
      For a more integrated workflow use Leiningen, Boot, or Maven.
    EOS
  end

  test do
    (testpath/"t.cljs").write <<~EOS
      (ns hello)
      (defn ^:export greet [n]
        (str "Hello " n))
    EOS

    system "#{bin}/cljsc", testpath/"t.cljs"
  end
end
