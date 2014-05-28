# Copyright 2014 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).

import condrv
import unittest


class ConDrvDataTest(unittest.TestCase):

  def test_ansi_buffer(self):
    foo = condrv.AnsiBuffer("foo")
    self.assertEquals(4, len(foo))
    self.assertEquals('A"foo\0"', repr(foo))
    self.assertEquals("foo", str(foo))
    s8 = condrv.AnsiBuffer(8)
    self.assertEquals(8, len(s8))
    self.assertEquals('A"\0\0\0\0\0\0\0\0"', repr(s8))
    self.assertEquals("", str(s8))
    self.assertRaises(TypeError, condrv.AnsiBuffer, [])

  def test_dword_ref(self):
    ref = condrv.DwordRef()
    self.assertEquals(0, ref.value)
    self.assertEquals("&0", str(ref))
    ref.value = 99
    self.assertEquals(99, ref.value)
    self.assertEquals("&99", str(ref))
    ref.value = -1
    self.assertEquals(-1, ref.value)
    self.assertEquals("&-1", str(ref))

if __name__ == '__main__':
  runner = unittest.TextTestRunner(verbosity=0)
  unittest.main(testRunner=runner)
