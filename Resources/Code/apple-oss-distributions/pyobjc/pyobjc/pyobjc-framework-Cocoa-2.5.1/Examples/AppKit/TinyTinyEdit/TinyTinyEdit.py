"""TinyTinyEdit -- A minimal Document-based Cocoa application."""
from Cocoa import *
from PyObjCTools import Signals
from PyObjCTools import AppHelper

class TinyTinyDocument(NSDocument):
    textView = objc.IBOutlet()
    path = None

    def windowNibName(self):
        return "TinyTinyDocument"

    def readFromFile_ofType_(self, path, tp):
        if self.textView is None:
            # we're not yet fully loaded
            self.path = path
        else:
            # "revert"
            self.readFromUTF8(path)
        return True

    def writeToFile_ofType_(self, path, tp):
        f = file(path, "w")
        text = self.textView.string()
        f.write(text.encode("utf8"))
        f.close()
        return True

    def windowControllerDidLoadNib_(self, controller):
        if self.path:
            self.readFromUTF8(self.path)

    def readFromUTF8(self, path):
        f = file(path)
        text = unicode(f.read(), "utf8")
        f.close()
        self.textView.setString_(text)


if __name__ == "__main__":
    AppHelper.runEventLoop()
