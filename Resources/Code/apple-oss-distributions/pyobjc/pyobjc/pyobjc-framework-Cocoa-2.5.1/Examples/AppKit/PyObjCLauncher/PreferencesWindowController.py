from Foundation import *
from AppKit import *
from PyObjCTools import NibClassBuilder
from FileSettings import *

class PreferencesWindowController(NSWindowController):
    commandline = objc.IBOutlet()
    debug = objc.IBOutlet()
    filetype = objc.IBOutlet()
    honourhashbang = objc.IBOutlet()
    inspect = objc.IBOutlet()
    interpreter = objc.IBOutlet()
    nosite = objc.IBOutlet()
    optimize = objc.IBOutlet()
    others = objc.IBOutlet()
    tabs = objc.IBOutlet()
    verbose = objc.IBOutlet()
    with_terminal = objc.IBOutlet()
    _singleton = None
    settings = None

    def getPreferencesWindow(cls):
        if not cls._singleton:
            cls._singleton = PreferencesWindowController.alloc().init()
        cls._singleton.showWindow_(cls._singleton)
        return cls._singleton
    getPreferencesWindow = classmethod(getPreferencesWindow)

    def init(self):
        return self.initWithWindowNibName_(u'PreferenceWindow')

    def load_defaults(self):
        title = self.filetype.titleOfSelectedItem()
        self.settings = FileSettings.getDefaultsForFileType_(title)

    def updateDisplay(self):
        if self.settings is None:
            return
        dct = self.settings.fileSettingsAsDict()
        self.interpreter.reloadData()
        self.interpreter.setStringValue_(dct['interpreter'])
        self.honourhashbang.setState_(dct['honourhashbang'])
        self.debug.setState_(dct['debug'])
        self.verbose.setState_(dct['verbose'])
        self.inspect.setState_(dct['inspect'])
        self.optimize.setState_(dct['optimize'])
        self.nosite.setState_(dct['nosite'])
        self.tabs.setState_(dct['tabs'])
        self.others.setStringValue_(dct['others'])
        self.with_terminal.setState_(dct['with_terminal'])
        self.commandline.setStringValue_(self.settings.commandLineForScript_(u'<your script here>'))

    def windowDidLoad(self):
        super(PreferencesWindowController, self).windowDidLoad()
        try:
            self.load_defaults()
            self.updateDisplay()
        except:
            import traceback
            traceback.print_exc()
            import pdb, sys
            pdb.post_mortem(sys.exc_info()[2])

    def updateSettings(self):
        self.settings.updateFromSource_(self)

    @objc.IBAction
    def doFiletype_(self, sender):
        self.load_defaults()
        self.updateDisplay()

    @objc.IBAction
    def doReset_(self, sender):
        self.settings.reset()
        self.updateDisplay()

    @objc.IBAction
    def doApply_(self, sender):
        self.updateSettings()
        self.updateDisplay()

    def fileSettingsAsDict(self):
        return dict(
            interpreter=self.interpreter.stringValue(),
            honourhashbang=self.honourhashbang.state(),
            debug=self.debug.state(),
            verbose=self.verbose.state(),
            inspect=self.inspect.state(),
            optimize=self.optimize.state(),
            nosite=self.nosite.state(),
            tabs=self.tabs.state(),
            others=self.others.stringValue(),
            with_terminal=self.with_terminal.state(),
            scriptargs=u'',
        )

    def controlTextDidChange_(self, aNotification):
        self.updateSettings()
        self.updateDisplay()

    def comboBox_indexOfItemWithStringValue_(self, aComboBox, aString):
        if self.settings is None:
            return -1
        dct = self.settings.fileSettingsAsDict()
        return dct['interpreter_list'].indexOfObject_(aString)

    def comboBox_objectValueForItemAtIndex_(self, aComboBox, index):
        if self.settings is None:
            return None
        return self.settings.fileSettingsAsDict()['interpreter_list'][index]

    def numberOfItemsInComboBox_(self, aComboBox):
        if self.settings is None:
            return 0
        return len(self.settings.fileSettingsAsDict()['interpreter_list'])
