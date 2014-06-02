# Console proxy

The console proxy is responsible for the actual replacement of the console.  It
handles the interaction with the underlying system and turns interaction with
the windows console api into messages with the replacement console
implementation. The windows console api was *not* designed to be replaced so the
proxy is somewhat involved and has a number of moving parts. Everything is there
for a reason though, as outlined below.

## Objective

Unlike the various flavors of unix's consoles, the windows [console
api](http://msdn.microsoft.com/en-us/library/windows/desktop/ms682010(v=vs.85).aspx)
was not designed to be replaced. If you want to write to the current console you
call `WriteConsole` which is hardcoded to write using the built-in console
infrastructure. This is one reason the console hasn't evolved at all on windows.

That doesn't mean it's impossible to replace it, only quite difficult.  Being
hardcoded is relative. With the right permissions code is mutable, and on
windows you can patch even built-in apis. This is an established practice --
thought somewhat questionable -- on
[windows](https://en.wikipedia.org/wiki/DLL_injection#Approaches_on_Microsoft_Windows).
The proxy uses this to patch the entire console api for each process on startup.
The component doing this is known as the *agent*, since it becomes a component
of your program that works on behalf of the proxy.

Since the agent runs within all your processes it is important that it is
ligheweight and secure. It is also more difficult to update and distribute, a
new version is likely to require a restart, so having as little code as possible
is appealing. For this reason the agent does only patching and delegates all
calls to a different process, the *handler*. The handler is the actual console
implementation. This is not that different from how the built- in console works,
the console api generally translates operations into messages to the console
host, `conhost.exe`.

## Agent

The agent is implemented as a DLL that is loaded into all new processes. It
identifies all the console functions that need to be replaced and then patches
them such that when they're called, instead of the built-in behavior the call is
redirected to the agent which can forward it appropriately.

See the details of how this works in {{agent.hh}}.
