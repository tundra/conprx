# Copyright 2014 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).

import condrv
import unittest


class ConsoleDriverTest(unittest.TestCase):

  def test_whatever(self):
    foo = condrv.AnsiString("foo")
    self.assertEquals(4, len(foo))
    self.assertEquals('A"foo"', str(foo))


if __name__ == '__main__':
  runner = unittest.TextTestRunner(verbosity=0)
  unittest.main(testRunner=runner)
