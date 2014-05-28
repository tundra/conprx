# Copyright 2014 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).

import condrv
import unittest


class ConDrvConsoleTest(unittest.TestCase):

  def test_console(self):
    console = condrv.Console()
    if console is None:
      # This is a platform that doesn't have a console.
      return
    buf = condrv.AnsiBuffer(256)
    self.assertRaises(TypeError, console.GetConsoleTitleA, buf)
    self.assertRaises(TypeError, console.GetConsoleTitleA, [], 256)
    print console.SetConsoleTitleA(condrv.AnsiBuffer("Looky here!"))
    print console.GetConsoleTitleA(buf, 256)
    print "{{{%s}}}" % buf

if __name__ == '__main__':
  runner = unittest.TextTestRunner(verbosity=0)
  unittest.main(testRunner=runner)
