# PowerToys for UltraWides

This repo contains my personal fork of PowerToys, tweaked to play better on UltraWide monitors.

Please see the official project page, [Microsoft PowerToys](https://github.com/microsoft/PowerToys/), to learn more about PowerToys.

## New Features
- "Maximize In Zone", which overrides window maximize behavior to keep windows sized within the zone you placed them in
   - The behavior can be overridden by holding "Shift" while maximizing
   - See discussion [here](https://github.com/microsoft/PowerToys/issues/279) for further details
- "Auto Zoning" all currently unzoned windows when starting/initializing PowerToys
- "Auto Zoning" any new windows, which have never been previously zoned, to the currently active zone (zone under the mouse cursor)
- "Auto Zoning" new child windows over their parent window, in conjunction with "last known zone", if their parent window is currently active
   - Detailed rationale for this feature here: [fdf5597](https://github.com/peddamat/PowerToys/commit/fdf55970fb9138a62aa2d2c3fc0be0e209919954)

## Maximize In Zone

Below is a rough video on how the feature works:

[![Screenrecording]](https://user-images.githubusercontent.com/869300/170856719-f9ea23c1-6159-49d9-95b3-89b6c1e0f6a7.mp4)

- The first part of the video demonstrates the current behavior.
- Then, it shows the flag being enabled in the Settings window.
- Then it demonstrates the new behavior, first via clicking on the "Maximize" button, then via double-clicks on the title bar.
- Finally, I demonstrate the "override" behavior, by hitting the "Shift" key and maximizing the window in various ways.

## Caution

The code should be considered alpha, so use at your own risk.  Even better, take a gander at the code to make sure it's sane.