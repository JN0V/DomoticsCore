# Deferred work

Decisions a lot parked rather than took, each with its evidence, until a lot
takes it. **A finding with a visible consequence does not belong here**: it
takes a roadmap id when it is written — the lot that opens it files it — and
a note or a negative result goes to the component's `project-context.md`.
This file holds the third kind only: a question whose answer is a design
choice nobody has made yet.

The rule dates from 2026-09-20. The file then held 62 entries, 5 of them
ever closed and 42 added in the fifteen days before; read one by one that
day, 34 became nineteen roadmap items, 5 were folded into the entries that
had parked them, 6 became notes, 15 were deleted as done or already
recorded, and the two below stayed.

- **Hashing the stored WebUI and console passwords.** Both are stored as
  entered; hashing them is a migration of the Storage keys that hold them,
  and a decision on what a device with no other way to set a password does
  when the stored form changes — a design question, not a patch. Parked by
  the SEC-4 / SEC-6 / SEC-14 lot, 2026-09-14.

- **`-ffile-prefix-map=<repo root>=` in the ESP32 build flags.** An ESP32
  image embeds the source paths the core's log macros expand from
  `__FILE__` — 30 strings in FullStack `esp32dev` — and with `symlink://`
  they are the components' real, absolute paths, so the image and its flash
  figure depend on where the checkout lives (+864 bytes locally against the
  old `.pio/libdeps/...` paths). The flag makes the image path-independent,
  and changes what a crash log names. Parked by CI-15's lot, 2026-09-15.

- **Whether a WebUI handler runs on the loop's task, and whether the code has
  to say so.** On an ESP32, ESPAsyncWebServer dispatches in the AsyncTCP task,
  so a provider handler writes component state while `loop()` reads and writes
  it on the other task — `handleWebUIRequest()` calls `setSTACredentials()` and
  `startScanAsync()`, and the latter assigns an Arduino `String` the scan poll
  assigns too. The file's existing deferrals (`pendingModeUpdate_`,
  `pendingConfigSave_`) were written for heap, not for that, and every provider
  in the repository writes straight through. The decision is whether the
  contract is "handlers run on another task, defer every write" or "handlers may
  write", and it is repository-wide rather than one component's. Raised by
  BUG-47's review, 2026-09-21.
