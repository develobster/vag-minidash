# MiniDash embedded UI settings

## Navigation

Touch gestures and the USB serial console both feed the same semantic actions
used by the UI input controller. Anywhere on the screen is a gesture target:

| Touch gesture | Normal screen | Settings screen |
| --- | --- | --- |
| Tap | Next screen | Next settings row |
| Double-tap | Previous screen | Change selected option |
| Hold for 700 ms | Open settings | Return to previous screen |

Touch while the display is sleeping wakes it. Gesture navigation does not use
touch coordinates, so it does not depend on the current uncalibrated XPT2046
position mapping.

The USB serial console accepts these commands:

| Command | Action |
| --- | --- |
| `next` / `prev` | Move between screens |
| `settings` | Open settings |
| `up` / `down` | Move the selected settings row |
| `plus` / `minus` | Change the selected setting |
| `select` | Increase the selected setting |
| `back` | Return from settings or go to the dashboard |
| `sleep` / `wake` | Turn the display panel off or on |

The XPT2046 touch input supports full-screen gestures. Coordinate calibration
and location-based controls are still open. No physical buttons, stalk or
steering-wheel controller is connected in this implementation.

## On-screen options

The settings screen offers:

| Option | Values |
| --- | --- |
| Dashboard | OEM, Sport, Minimal, Custom |
| Theme | OEM, Sport, Minimal |
| Day / night | Auto, Day, Night |
| Speed units | km/h, mph |
| Temperature | °C, °F |
| Brightness | 0–100%, in cyclic 10% steps |
| Warnings | Enabled or disabled |

Auto day/night currently selects the night-safe palette because no ambient-light
or vehicle-context provider is connected. Brightness is persisted, but the
current board backlight is wired directly to 3.3 V, so the diagnostics screen
reports fixed brightness and software cannot dim or extinguish the LED.

Warnings are off by default. Turning them on only enables warning thresholds
that have also been configured. The firmware API accepts threshold values in
thousandths of each signal's canonical unit. For example, a coolant threshold
of 105 °C is `105000` when calling `md_display_app_set_warning()`. Thresholds
are evaluated only for fresh valid samples; stale or invalid values do not
raise a warning.

## Custom dashboard

The custom dashboard can render up to six value widgets on a 12-column by
8-row grid. Widgets must be at least 4 columns by 4 rows and cannot overlap.
They bind to the supported canonical signal names documented in
[the UI architecture](architecture.md). The default custom layout uses four
equal tiles for RPM, speed, coolant temperature and engine oil temperature.

There is no on-screen editor for changing widget positions or bindings yet.
The settings model and persistence format validate custom bindings, but the
current screen only selects the Custom dashboard preset.

## Persistence and reset behavior

Settings are stored in NVS after 1.5 seconds without another settings change.
They survive reboot. Missing or invalid records use defaults; invalid records
are not erased automatically. The NVS partition is declared in
`firmware/partitions.csv` alongside the larger 3 MiB factory app partition.
