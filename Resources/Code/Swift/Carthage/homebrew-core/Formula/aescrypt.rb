class Aescrypt < Formula
  desc "Program for encryption/decryption"
  homepage "https://aescrypt.sourceforge.io/"
  url "https://aescrypt.sourceforge.io/aescrypt-0.7.tar.gz"
  sha256 "7b17656cbbd76700d313a1c36824a197dfb776cadcbf3a748da5ee3d0791b92d"

  bottle do
    cellar :any_skip_relocation
    sha256 "e41505ebcf2ca60292fd7391501ccc8d81ec41c96b23f2f50f21315bafc97f77" => :big_sur
    sha256 "a6ca5e29be88eea7f2fe4faf1e57e3f827bfa86bae2726e5f83cedc79c091fcb" => :arm64_big_sur
    sha256 "c5dac9eb7f3ce8509c766d82ef5f972c8a41984284ae3e01651c6f308164c5bd" => :catalina
    sha256 "55bc9c5be0263f1659ab389b22e1e5f594b037a87d49aa5ed94ab5ccce3af3da" => :mojave
    sha256 "1b2326e6dbc73d394cb5d4d7bf655b026fa77a7d66d02da386bff16b84e84d83" => :high_sierra
    sha256 "2250bd07f689721287269dc70c504b4f08ac2a02b5550ad9f0a51dca60ed6f9a" => :sierra
    sha256 "0cd940c7c9e59104746a8f83f92a06e703e7f98195a202d20516c03b588fd63f" => :el_capitan
    sha256 "660c8a9266d7f85e699fb5bfabb82c508a66d303b2a2057c9c70a3c70fed43f6" => :yosemite
    sha256 "a0bf8895165037991bf5b33be5c995e9b68a1d05898003a0ef45adb7aa3d3da9" => :mavericks
  end

  def install
    system "./configure"
    system "make"
    bin.install "aescrypt", "aesget"
  end

  test do
    (testpath/"key").write "kk=12345678901234567890123456789abc0"
    original_text = "hello"
    cipher_text = pipe_output("#{bin}/aescrypt -k #{testpath}/key -s 128", original_text)
    deciphered_text = pipe_output("#{bin}/aesget -k #{testpath}/key -s 128", cipher_text)
    assert_not_equal original_text, cipher_text
    assert_equal original_text, deciphered_text
  end
end
