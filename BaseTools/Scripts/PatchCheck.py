#  Copyright (c) 2020, ARM Ltd. All rights reserved.<BR>
                if self.filename.endswith('.sh') or \
                    self.filename.startswith('BaseTools/BinWrappers/PosixLike/') or \
                    self.filename.startswith('BaseTools/Bin/CYGWIN_NT-5.1-i686/') or \
                    self.filename == 'BaseTools/BuildEnv':
                    # Some linux shell scripts don't end with the ".sh" extension,
                    # they are identified by their path.