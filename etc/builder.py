from iocbuilder import Device
from iocbuilder.iocinit import quote_IOC_string

class PvLogging(Device):
    LibFileList = ['pvlogging']
    DbdFileList = ['pvlogging']
    AutoInstantiate = True

    def __init__(self, access_file = None):
        self.__super.__init__()
        if access_file is None:
            self.access_file = self.ModuleFile('data/access.acf')
        else:
            self.access_file = access_file

    def InitialiseOnce(self):
        print 'asSetFilename %s' % quote_IOC_string(self.access_file)
        print 'InstallPvPutHook'
