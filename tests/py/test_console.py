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

if __name__ == '__main__':
  runner = unittest.TextTestRunner(verbosity=0)
  unittest.main(testRunner=runner)
