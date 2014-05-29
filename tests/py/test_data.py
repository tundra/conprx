# Copyright 2014 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).

import condrv
import unittest


class ConDrvDataTest(unittest.TestCase):

  def test_ansi_buffer(self):
    foo = condrv.AnsiBuffer("foo")
    self.assertEquals(4, len(foo))
    self.assertEquals("A'foo\x00'", repr(foo))
    self.assertEquals("foo", str(foo))
    s8 = condrv.AnsiBuffer(8)
    self.assertEquals(8, len(s8))
    self.assertEquals("A'\x00\x00\x00\x00\x00\x00\x00\x00'", repr(s8))
    self.assertEquals("", str(s8))
    self.assertRaises(TypeError, condrv.AnsiBuffer, [])

  def test_wide_buffer(self):
    foo = condrv.WideBuffer(u"foo")
    self.assertEquals(4, len(foo))
    self.assertEquals("Uu'foo\\x00'", repr(foo))
    self.assertEquals("foo", unicode(foo))
    self.assertEquals("foo", str(foo))
    self.assertEquals(ord("f"), foo[0])
    self.assertEquals(ord("o"), foo[1])
    self.assertEquals(ord("o"), foo[2])
    self.assertRaises(IndexError, lambda: foo[4])
    self.assertEquals(0, foo[3])
    s8 = condrv.WideBuffer(8)
    self.assertEquals(8, len(s8))
    self.assertEquals("Uu'\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00'", repr(s8))
    self.assertEquals("", str(s8))
    for i in range(0, 8):
      self.assertEquals(0, s8[i])
    self.assertRaises(TypeError, condrv.WideBuffer, [])
    runic = condrv.WideBuffer(u"\u1681\u1693")
    self.assertEquals(3, len(runic))
    self.assertEquals("Uu'\\u1681\\u1693\\x00'", repr(runic))
    self.assertEquals(u"\u1681\u1693", unicode(runic))
    self.assertEquals(0x1681, runic[0])
    self.assertEquals(0x1693, runic[1])
    self.assertEquals(0, runic[2])
    self.assertRaises(IndexError, lambda: runic[3])
    combined = condrv.WideBuffer(u"\U00010A60")
    self.assertEquals(3, len(combined))
    self.assertEquals("Uu'\\U00010a60\\x00'", repr(combined))
    self.assertEquals(u"\U00010A60", unicode(combined))
    self.assertEquals(0xD802, combined[0])
    self.assertEquals(0xDE60, combined[1])
    self.assertEquals(0, combined[2])
    self.assertRaises(IndexError, lambda: combined[3])

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
