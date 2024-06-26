#!/usr/bin/env python2
# Copyright (c) 2016-2017 The Duality Blockchain Solutions developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

#
# Exercise API with -disablewallet.
#

from test_framework.test_framework import CashTestFramework
from test_framework.util import *


class DisableWalletTest (CashTestFramework):

    def setup_chain(self):
        print("Initializing test directory "+self.options.tmpdir)
        initialize_chain_clean(self.options.tmpdir, 1)

    def setup_network(self, split=False):
        self.nodes = start_nodes(1, self.options.tmpdir, [['-disablewallet']])
        self.is_network_split = False
        self.sync_all()

    def run_test (self):
        # Check regression: https://github.com/cash/cash/issues/6963#issuecomment-154548880
        x = self.nodes[0].validateaddress('7TSBtVu959hGEGPKyHjJz9k55RpWrPffXz')
        assert(x['isvalid'] == False)
        x = self.nodes[0].validateaddress('ycwedq2f3sz2Yf9JqZsBCQPxp18WU3Hp4J')
        assert(x['isvalid'] == True)

if __name__ == '__main__':
    DisableWalletTest ().main ()
