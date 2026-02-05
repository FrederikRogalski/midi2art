# MIDI2Art Plugin Testing Guide

## Debugging

The plugin now includes comprehensive logging. Check these locations:

1. **Console Output**: When running in a DAW, check the Console app (Applications > Utilities > Console.app)
   - Filter for "MIDI2Art" or "MIDI2Art_Debug"

2. **Log File**: `~/Desktop/MIDI2Art_Debug.log`
   - Contains all debug messages
   - Created automatically when plugin loads

## What Gets Logged

- Plugin initialization
- MIDI note on/off events
- Art-Net packet sends
- Parameter changes
- Errors and warnings

## Testing Steps

1. **Rebuild the plugin** (the .jucer file was updated to make it an instrument):
   ```bash
   cd Builds/MacOSX
   xcodebuild -project MIDI2Art.xcodeproj -scheme "MIDI2Art - AU" -configuration Release clean build
   ```

2. **Restart your DAW** completely (quit and reopen)

3. **Load the plugin** as an Instrument (not an effect)

4. **Check the log file** on your Desktop to see what's happening

5. **Send MIDI notes** and watch the log for:
   - "MIDI Note On" messages
   - "updateArtNetOutput" messages
   - Art-Net send confirmations

## Common Issues

### Plugin doesn't appear as Instrument
- Make sure you regenerated the Xcode project from the .jucer file
- Or manually open the .jucer in Projucer and click "Save and Open in IDE"

### No MIDI messages received
- Check that MIDI routing is correct in your DAW
- Look for "processBlock: Received X MIDI events" in the log

### Art-Net not sending
- Check IP address is correct
- Verify WLED is on the same network
- Look for "updateArtNetOutput: Skipped" messages in log

