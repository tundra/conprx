# Copyright 2014 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).

import condrv
import unittest


class ConsoleDriverTest(unittest.TestCase):

  def test_whatever(self):
    foo = condrv.AnsiBuffer("foo")
    self.assertEquals(4, len(foo))
    self.assertEquals('A"foo\0"', repr(foo))
    self.assertEquals("foo", str(foo))
    s8 = condrv.AnsiBuffer(8)
    self.assertEquals(8, len(s8))
    self.assertEquals('A"\0\0\0\0\0\0\0\0"', repr(s8))
    self.assertEquals("", str(s8))
    self.assertRaises(TypeError, condrv.AnsiBuffer, [])

if __name__ == '__main__':
  runner = unittest.TextTestRunner(verbosity=0)
  unittest.main(testRunner=runner)
