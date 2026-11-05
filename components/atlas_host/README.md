# components/atlas_host

The browser's side of the bridge to the WolOS desktop (owner: ATLAS-HOST). New BSD-3-Clause code with no
WolOS source in it: the contract is the **Atlas Host Protocol**, version 2, a line-delimited JSON wire over
`$XDG_RUNTIME_DIR/atlas/bridge.sock`, documented on the WolOS side in `docs/modules/atlas.md`.

| File | What |
|---|---|
| `protocol.{h,cc}` | the codec: requests, replies, the path rule, `LineReader` (lines of at most 32 KiB) |
| `atlas_host.{h,cc}` | `AtlasHost`: connects (without blocking), says hello, sends a verb with an id and returns what the other end answers; reconnects; texts when no answer comes |
| `download_verbs.{h,cc}` | what a downloads menu needs: the three command ids, "offer them?", "run one" |
| `atlas_host_prefs.{h,cc}` | the area's profile prefs (none yet) |
| `testdata/` | the fixtures of the protocol, **identical byte for byte** to `crates/shared/tests/fixtures/atlas/` in the WolOS repository (`cmp -r` them when either side moves) |

What it does: three verbs on a file, `share`, `open_with`, `trash`. What it does not: anything else. It
holds no logic of the desktop (the share sheet, the app chooser and the Trash are the desktop's), opens no
file, makes no network connection (one `AF_UNIX` socket), never writes a path to a log, and offers
nothing when the socket is missing or the other end does not answer the hello (`IsAvailable()`).

## Behaviour worth knowing

- The bridge speaks first: it writes `hello <nonce>` as soon as it accepts, and nothing is written before
  that. The host writes the same line back, then the hello, in one write (ADR 0033, `EchoOfGreeting`);
  the kernel stamps the echo with the writing process, so only the process that connected can answer.
  A bridge that is over its limit answers with an error in place of the greeting.
- The hello is `{"op":"hello","version":2,"pid":<this process>}`; the other end checks the pid against the
  kernel's and the executable against `/opt/atlas/atlas`. The greeting and the hello share the hello
  timeout.
- At most 4 verbs wait at once on a connection; a fifth is answered `busy` here without being sent.
- A verb is never given up on early: Share and Open With wait up to 10 minutes (the person is choosing in
  a sheet), Trash 30 seconds. After that the connection is closed and opened again, and the person reads
  "WolOS did not answer in time. Try again in a moment."
- End of stream before the hello is read means the bridge was leaving as we arrived (it exits when idle and
  the socket starts a new one): the host connects again at once, then after 1, 2, 4, 8, 16 seconds, then
  waits for the next `EnsureConnected()` (a menu opening) or verb. A missing socket is not retried by timer.
- Replies are delivered on the sequence that created the host, never from inside the call that asked.

## Tests

`out/Default/atlas_host_unittests --gtest_filter='*AtlasHost*'` builds just this component's tests and
runs them: the fixtures (valid lines decode and re-encode to the same bytes, invalid ones are refused),
every session transcript of the bridge played from the browser's side against a fake bridge, reconnect,
timeouts, the 4-in-flight limit, replies out of order. The same tests are in `unit_tests`
(`//components/atlas_host:unit_tests`).
