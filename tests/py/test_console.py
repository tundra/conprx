# Copyright 2014 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).

from condrv import *
import unittest


class ConDrvConsoleTest(unittest.TestCase):

  def test_console(self):
    console = Console()
    if console is None:
      # This is a platform that doesn't have a console.
      return
    buf = AnsiBuffer(256)
    self.assertRaises(TypeError, console.GetConsoleTitleA, buf)
    self.assertRaises(TypeError, console.GetConsoleTitleA, [], 256)
    self.assertRaises(TypeError, console.SetConsoleTitleA)
    self.assertRaises(TypeError, console.SetConsoleTitleA, [])
    self.assertEquals(-10, console.STD_INPUT_HANDLE)
    self.assertEquals(-11, console.STD_OUTPUT_HANDLE)
    self.assertEquals(-12, console.STD_ERROR_HANDLE)
    stderr = console.GetStdHandle(console.STD_ERROR_HANDLE)

  def test_ansi_titles(self):
    console = Console()
    if console is None:
      return
    abuf = AnsiBuffer(256)
    asize = 256
    wbuf = WideBuffer(256)
    wsize = 512
    # ""
    self.assertTrue(console.SetConsoleTitleA(AnsiBuffer("")))
    self.assertEquals(0, console.GetConsoleTitleA(abuf, asize))
    self.assertEquals("", str(abuf))
    # "TITLE"
    self.assertTrue(console.SetConsoleTitleA(AnsiBuffer("TITLE")))
    self.assertEquals(5, console.GetConsoleTitleA(abuf, asize))
    self.assertEquals("TITLE", str(abuf))
    self.assertEquals(5, console.GetConsoleTitleW(wbuf, wsize))
    self.assertEquals(u"TITLE", unicode(wbuf))
    # "QUITEALONGTITLE"
    self.assertTrue(console.SetConsoleTitleA(AnsiBuffer("QUITEALONGTITLE")))
    self.assertEquals(0, console.GetConsoleTitleA(abuf, 8))
    self.assertEquals("", str(abuf))
    self.assertEquals(15, console.GetConsoleTitleW(wbuf, 16))
    self.assertEquals(u"QUITEAL", unicode(wbuf))
    self.assertEquals(15, console.GetConsoleTitleW(wbuf, 17))
    self.assertEquals(u"QUITEAL", unicode(wbuf))
    self.assertEquals(15, console.GetConsoleTitleW(wbuf, 18))
    self.assertEquals(u"QUITEALO", unicode(wbuf))
    self.assertEquals(15, console.GetConsoleTitleW(wbuf, 8))
    self.assertEquals(u"QUI", unicode(wbuf))
    self.assertEquals(15, console.GetConsoleTitleA(abuf, asize))
    self.assertEquals("QUITEALONGTITLE", str(abuf))
    self.assertEquals(15, console.GetConsoleTitleW(wbuf, wsize))
    self.assertEquals(u"QUITEALONGTITLE", unicode(wbuf))

  def test_wide_titles(self):
    console = Console()
    if console is None:
      return
    abuf = AnsiBuffer(256)
    asize = 256
    wbuf = WideBuffer(256)
    wsize = 512
    # ""
    self.assertTrue(console.SetConsoleTitleW(WideBuffer(u"")))
    self.assertEquals(0, console.GetConsoleTitleA(abuf, asize))
    self.assertEquals("", str(abuf))
    self.assertEquals(0, console.GetConsoleTitleW(wbuf, wsize))
    self.assertEquals(u"", unicode(wbuf))
    # "TITLE"
    self.assertTrue(console.SetConsoleTitleW(WideBuffer(u"TITLE")))
    self.assertEquals(5, console.GetConsoleTitleA(abuf, asize))
    self.assertEquals("TITLE", str(abuf))
    self.assertEquals(5, console.GetConsoleTitleW(wbuf, wsize))
    self.assertEquals(u"TITLE", unicode(wbuf))
    # "QUITEALONGTITLE"
    self.assertTrue(console.SetConsoleTitleW(WideBuffer(u"QUITEALONGTITLE")))
    self.assertEquals(15, console.GetConsoleTitleW(wbuf, 16))
    self.assertEquals(u"QUITEAL", unicode(wbuf))
    self.assertEquals(15, console.GetConsoleTitleW(wbuf, 17))
    self.assertEquals(u"QUITEAL", unicode(wbuf))
    self.assertEquals(15, console.GetConsoleTitleW(wbuf, 18))
    self.assertEquals(u"QUITEALO", unicode(wbuf))
    self.assertEquals(15, console.GetConsoleTitleW(wbuf, wsize))
    self.assertEquals(u"QUITEALONGTITLE", unicode(wbuf))
    # "-[\u16c4]-"
    s = u"-[\u16c4]-"
    self.assertTrue(console.SetConsoleTitleW(WideBuffer(s)))
    self.assertEquals(5, console.GetConsoleTitleW(wbuf, wsize))
    self.assertEquals(s, unicode(wbuf))
    self.assertEquals(5, console.GetConsoleTitleA(abuf, asize))
    self.assertEquals("-[?]-", str(abuf))
    # "-{\U00010A60}-"
    s = u"-{\U00010A60}-"
    self.assertTrue(console.SetConsoleTitleW(WideBuffer(s)))
    self.assertEquals(6, console.GetConsoleTitleW(wbuf, 6))
    self.assertEquals(u"-{", unicode(wbuf))
    self.assertEquals(6, console.GetConsoleTitleW(wbuf, 7))
    self.assertEquals(u"-{", unicode(wbuf))
    self.assertEquals(6, console.GetConsoleTitleW(wbuf, 8))
    self.assertRaises(UnicodeDecodeError, lambda: unicode(wbuf))
    self.assertEquals(6, console.GetConsoleTitleW(wbuf, 9))
    self.assertRaises(UnicodeDecodeError, lambda: unicode(wbuf))
    self.assertEquals(6, console.GetConsoleTitleW(wbuf, 10))
    self.assertEquals(u"-{\U00010A60", unicode(wbuf))
    self.assertEquals(0, console.GetConsoleTitleA(abuf, 2))
    self.assertEquals("", str(abuf))
    self.assertEquals(0, console.GetConsoleTitleA(abuf, 3))
    self.assertEquals("", str(abuf))
    self.assertEquals(0, console.GetConsoleTitleA(abuf, 4))
    self.assertEquals("", str(abuf))

  def test_console_cursor_info(self):
    console = Console()
    if console is None:
      return
    info = ConsoleCursorInfo(0, False)
    conout = console.GetStdHandle(console.STD_ERROR_HANDLE)
    self.assertTrue(console.GetConsoleCursorInfo(conout, info))

if __name__ == '__main__':
  runner = unittest.TextTestRunner(verbosity=0)
  unittest.main(testRunner=runner)
