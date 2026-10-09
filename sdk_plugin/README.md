# Single-press body dumping

`NoHoldBodyDump.dll` is the optional ZHMModSDK companion for No Hold Buttons. It makes native body dumping while dragging use a single press. Keyboard uses your configured Open Door key; controller uses Interact/Pickup. The companion was confirmed working in-game by Charc0al.

Requires ZHMModSDK 4.1.1 and the supported HITMAN 3 executable below. SMF does not install this companion automatically.

1. Close the game.
2. Copy `NoHoldBodyDump.dll` and `NoHoldBodyDump.ini` into the game's `Retail\mods` directory.
3. Add an empty `[NoHoldBodyDump]` section to `Retail\mods.ini`, keeping the other sections, or enable NoHoldBodyDump in the SDK UI and restart the game.

Settings in `Retail\mods\NoHoldBodyDump.ini`:

```ini
[bodydump]
enabled = true
use_open_door_key = true
```

Set `use_open_door_key = false` to keep the original keyboard dump key with single press. Set `enabled = false` to disable the companion. Restart after changing settings.

The DLL checks the executable SHA-256 and all four original instruction sequences before changing process memory. It never edits the executable on disk. Supported executable: HITMAN 3 3.280, SHA-256 `005412cf59daa1aadfba8deb05f097f5418aec57ddc98216483d93bf5b0119b1`. Other builds are refused. VR is not covered; controller, alternate-key mode, and all dump-location variants have not been individually confirmed.

Source and build instructions are in [`sdk_plugin_src`](https://github.com/charc0al/HM3_NoHoldButtons/tree/main/sdk_plugin_src), retained in the Git repository and excluded from ZIP archives. Third-party notices are in `licenses`.
