class Plank < Formula
  desc "Framework for generating immutable model objects"
  homepage "https://pinterest.github.io/plank/"
  url "https://github.com/pinterest/plank/archive/v1.6.tar.gz"
  sha256 "6a233120905ff371b5c06a23b3fc7dd67e96355dd4d992a58ac087db22c500ef"
  license "Apache-2.0"
  head "https://github.com/pinterest/plank.git"

  livecheck do
    url :head
    regex(/^v?(\d+(?:\.\d+)+)$/i)
  end

  bottle do
    cellar :any_skip_relocation
    sha256 "ea5dbcccb44df98be951af22f29b81b24bdba7731f88a472708fe7c5bc3d53e3" => :big_sur
    sha256 "ba560dc8f11ecdeef3e745cc00a0b7f6cae8074d4cfff100f43227e548c5db7a" => :arm64_big_sur
    sha256 "fc6838079a8a975c9bb77d17a050aa722d8446fcf9f62ca9fe09c8822d8651b4" => :catalina
    sha256 "04d2dddb094914fa219304fea8f6e5aa3315c2e51b63ef4077fcf25a54c8b268" => :mojave
  end

  depends_on xcode: ["11.3", :build]

  def install
    system "make", "install", "PREFIX=#{prefix}"
  end

  test do
    (testpath/"pin.json").write <<~EOS
      {
        "id": "pin.json",
        "title": "pin",
        "description" : "Schema definition of a Pin",
        "$schema": "https://json-schema.org/schema#",
        "type": "object",
        "properties": {
          "id": { "type": "string" },
          "link": { "type": "string", "format": "uri"}
         }
      }
    EOS
    system "#{bin}/plank", "--lang", "objc,flow", "--output_dir", testpath, "pin.json"
    assert_predicate testpath/"Pin.h", :exist?, "[ObjC] Generated file does not exist"
    assert_predicate testpath/"PinType.js", :exist?, "[Flow] Generated file does not exist"
  end
end
